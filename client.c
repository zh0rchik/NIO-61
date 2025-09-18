/*
 client.c
 TCP Клиент с красивым выводом ответа:
 HEX, BIN, ADDR, SIGN, VALUE, V_MAX, DIGIT
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include "crc16.h"

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define PACKET_SIZE 12

// Печать 32-битного слова в бинарном виде с пробелами каждые 4 бита
static void print_binary32(uint32_t v) {
    for (int i = 31; i >= 0; i--) {
        printf("%d", (v >> i) & 1u);
        if (i % 4 == 0 && i != 0) printf(" ");
    }
    printf("\n");
}

// Расчёт весов разрядов (каждый следующий — в 2 раза меньше)
static void calculate_weights(uint16_t vmax, uint8_t digits, uint32_t weights[]) {
    uint32_t w = vmax;
    for (uint8_t i = 0; i < digits; i++) {
        weights[i] = w;
        w /= 2;
    }
}

// Разбор ответа сервера и вывод в удобном виде
void decode_response(uint32_t resp, uint16_t vmax, uint8_t digits) {
    int C = (resp >> 31) & 1;       // бит подтверждения (валидность ответа)
    int S = (resp >> 28) & 1;       // бит знака
    uint16_t addr = resp & 0x1FFFu; // 13 младших бит → адрес

    if (!C) {
        printf("Ошибка сервера: invalid request\n");
        return;
    }

    // Выборка «маски» включённых разрядов (digits бит подряд начиная с 27-го)
    uint32_t pattern = 0;
    for (uint8_t i = 0; i < digits; i++)
        if ((resp >> (27 - i)) & 1u)
            pattern |= (1u << (digits - 1 - i));

    // вычисляем веса разрядов
    uint32_t weights[16];
    calculate_weights(vmax, digits, weights);

    int32_t value = 0;
    if (!S) { // положительное число
        for (uint8_t i = 0; i < digits; i++)
            if ((pattern >> (digits - 1 - i)) & 1u)
                value += weights[i];
    } else { // отрицательное: сумма минус 2^digits
        int32_t sum = 0;
        for (uint8_t i = 0; i < digits; i++)
            if ((pattern >> (digits - 1 - i)) & 1u)
                sum += weights[i];
        value = sum - (1 << digits);
    }

    // Красивый вывод результата
    printf("\nОтвет от сервера:\n");
    printf("\tBIN: "); print_binary32(resp);
    printf("\t--------------------------------------------\n");
    printf("\tHEX: 0x%08X\n", resp);
    printf("\tADDR: %03o\n", addr); // восьмеричный формат
    printf("\tSIGN: %c\n", S ? '-' : '+');
    printf("\tVALUE: %d\n", value);
    printf("\tV_MAX: %u\n", vmax);
    printf("\tDIGIT: %u\n", digits);
}

// Вычисление параметров vmax и digits по введённому числу
void calculate_parameters(int value, uint16_t *vmax, uint8_t *digits) {
    int abs_val = abs(value);
    if (abs_val == 0) {
        *vmax = 1;
        *digits = 1;
        return;
    }

    int power = 0, tmp = abs_val;
    while (tmp > 0) { tmp >>= 1; power++; }
    if (value < 0) power++; // для отрицательных — добавляем 1 бит на знак

    *digits = (uint8_t)power;
    *vmax = 1u << (power - 1); // максимальное значение старшего разряда
}

// Ввод восьмеричного адреса с выравниванием до 3 цифр
int read_octal_address_and_fill(uint8_t packet[]) {
    char buf[16];
    printf("Адрес (восьмеричный): ");
    if (scanf("%15s", buf) != 1) return -1;

    size_t len = strlen(buf);
    if (len == 0 || len > 3) { printf("Неверный формат\n"); return -1; }

    for (size_t i = 0; i < len; i++)
        if (buf[i] < '0' || buf[i] > '7') { printf("Только цифры 0-7\n"); return -1; }

    char padded[4] = {'0','0','0','\0'};
    for (size_t i = 0; i < len; i++)
        padded[3 - len + i] = buf[i];

    packet[2] = padded[0] - '0';
    packet[3] = padded[1] - '0';
    packet[4] = padded[2] - '0';
    return 0;
}

// Сбор пакета данных на основе ввода пользователя
void get_user_input(uint8_t packet[]) {
    int value;
    uint16_t vmax;
    uint8_t digits;

    while (read_octal_address_and_fill(packet) != 0) {
        int c; while ((c = getchar()) != '\n' && c != EOF);
    }

    printf("Значение: ");
    while (scanf("%d", &value) != 1) {
        int c; while ((c = getchar()) != '\n' && c != EOF);
    }

    calculate_parameters(value, &vmax, &digits);

    uint16_t pid = (uint16_t)getpid();
    packet[0] = (pid >> 8) & 0xFF;
    packet[1] = pid & 0xFF;
    packet[5] = (value >> 8) & 0xFF;
    packet[6] = value & 0xFF;
    packet[7] = (vmax >> 8) & 0xFF;
    packet[8] = vmax & 0xFF;
    packet[9] = digits;
}

// Проверка доступности сервера
int check_server_alive(void) {
    int sock;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return 0; // сервер не доступен
    }

    close(sock);
    return 1; // сервер доступен
}

int main(void) {
    int sock;
    struct sockaddr_in serv_addr;
    uint8_t packet[PACKET_SIZE];
    char choice;

    printf("TCP Клиент %s:%d\n", SERVER_IP, PORT);

    // Проверяем сервер перед запуском
    if (!check_server_alive()) {
        printf("Сервер недоступен. Запустите server.c и повторите.\n");
        return -1;
    }

    do {
        memset(packet, 0, PACKET_SIZE);
        int ch; while ((ch = getchar()) != '\n' && ch != EOF); // очистка буфера ввода

        get_user_input(packet);

        uint16_t crc = crc16(packet, 10);
        packet[10] = (crc >> 8) & 0xFF;
        packet[11] = crc & 0xFF;

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { perror("socket"); return -1; }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
            perror("inet_pton"); close(sock); return -1;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("connect"); close(sock); continue;
        }

        if (send(sock, packet, PACKET_SIZE, 0) < 0) { perror("send"); close(sock); continue; }

        uint8_t resp_buf[64];
        ssize_t n = read(sock, resp_buf, sizeof(resp_buf));
        if (n >= 4) {
            uint32_t resp = (resp_buf[0] << 24) | (resp_buf[1] << 16) |
                            (resp_buf[2] << 8) | resp_buf[3];
            decode_response(resp, (packet[7]<<8)|packet[8], packet[9]);
        } else if (n > 0) {
            resp_buf[n] = '\0';
            printf("%s\n", resp_buf);
        } else if (n < 0) perror("read");
        else printf("Сервер закрыл соединение\n");

        close(sock);

        printf("\nЕщё запрос? (y/n): ");
        if (scanf(" %c", &choice) != 1) break;
        int c; while ((c = getchar()) != '\n' && c != EOF);
        printf("\n");
    } while (choice == 'y' || choice == 'Y');

    printf("Завершение работы.\n");
    return 0;
}
