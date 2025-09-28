/* client.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <math.h>
#include "crc16.h"

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define PACKET_SIZE 12
#define EPS 1e-9

static void print_binary32(uint32_t v) {
    for (int i = 31; i >= 0; --i) {
        printf("%d", (v >> i) & 1u);
        if (i % 4 == 0 && i != 0) printf(" ");
    }
    printf("\n");
}

/* Формирует массив весов как double: Vmax, Vmax/2, Vmax/4, ... */
static void calculate_weights_double(uint16_t vmax, uint8_t digits, double weights[]) {
    double w = (double)vmax;
    for (uint8_t i = 0; i < digits; ++i) {
        weights[i] = w;
        w /= 2.0;
    }
}

/* Декодирование 32-битного ответа */
void decode_response(uint32_t resp, uint16_t vmax, uint8_t digits) {
    int C = (resp >> 31) & 1;

    if (!C) {
        printf("Ошибка сервера: invalid request\n");
        return;
    }

    if (digits == 0 || digits > 27) {
        printf("Неверное значение digits в ответе: %u\n", digits);
        return;
    }

    uint16_t addr = resp & 0x1FFFu;

    /* Собираем pattern (в виде целого числа X) */
    uint32_t pattern = 0;
    for (uint8_t i = 0; i < digits; ++i) {
        if ((resp >> (27 - i)) & 1u) {
            pattern |= (1u << (digits - 1 - i));
        }
    }

    /* Получаем signed integer S_int из pattern */
    uint32_t half = (1u << (digits - 1));
    uint32_t full = (1u << digits);
    int32_t s_int;
    if (pattern & half) {
        s_int = (int32_t)pattern - (int32_t)full;
    } else {
        s_int = (int32_t)pattern;
    }

    double scale = (double)vmax / (double)half;
    double value_real = (double)s_int * scale;
    long value_rounded = llround(value_real);

    /* Печать ответа */
    printf("\nОтвет от сервера:\n");
    printf("\tBIN: "); print_binary32(resp);
    printf("\t--------------------------------------------\n");
    printf("\tHEX: 0x%08X\n", resp);
    printf("\tADDR: %03o\n", addr);
    printf("\tSIGN: %c\n", (s_int < 0) ? '-' : '+');
    printf("\tVALUE: %ld\n", value_rounded);
    printf("\tV_MAX: %u\n", vmax);
    printf("\tDIGIT: %u\n", digits);
}

/* Ввод восьмеричного адреса (3 цифры max), заполняем packet[2..4] */
int read_octal_address_and_fill(uint8_t packet[]) {
    char buf[16];
    printf("Адрес (восьмеричный, 000..377): ");
    if (scanf("%15s", buf) != 1) return -1;

    size_t len = strlen(buf);
    if (len == 0 || len > 3) { printf("Неверный формат\n"); return -1; }

    for (size_t i = 0; i < len; ++i)
        if (buf[i] < '0' || buf[i] > '7') { printf("Только цифры 0-7\n"); return -1; }

    char padded[4] = {'0','0','0','\0'};
    for (size_t i = 0; i < len; ++i) padded[3 - len + i] = buf[i];

    packet[2] = padded[0] - '0';
    packet[3] = padded[1] - '0';
    packet[4] = padded[2] - '0';
    return 0;
}

void get_user_input(uint8_t packet[]) {
    int value;
    uint16_t vmax;
    uint8_t digits;

    while (read_octal_address_and_fill(packet) != 0) {
        int c; while ((c = getchar()) != '\n' && c != EOF);
    }

    printf("Значение (целое, -32768..32767): ");
    while (scanf("%d", &value) != 1) { int c; while ((c = getchar()) != '\n' && c != EOF); }

    printf("Цена старшего разряда (Vmax, >0): ");
    while (scanf("%hu", &vmax) != 1 || vmax == 0) { int c; while ((c = getchar()) != '\n' && c != EOF); }

    printf("Количество значащих разрядов (digits, 1..27): ");
    while (scanf("%hhu", &digits) != 1 || digits == 0 || digits > 27) { int c; while ((c = getchar()) != '\n' && c != EOF); }

    uint16_t pid = (uint16_t)getpid();
    packet[0] = (pid >> 8) & 0xFF;
    packet[1] = pid & 0xFF;
    packet[5] = (value >> 8) & 0xFF;
    packet[6] = value & 0xFF;
    packet[7] = (vmax >> 8) & 0xFF;
    packet[8] = vmax & 0xFF;
    packet[9] = digits;
}

/* Проверка доступности сервера */
int check_server_alive(void) {
    int sock;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) { close(sock); return -1; }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { close(sock); return 0; }

    close(sock);
    return 1;
}

int main(void) {
    int sock;
    struct sockaddr_in serv_addr;
    uint8_t packet[PACKET_SIZE];
    char choice;

    printf("TCP Клиент %s:%d\n", SERVER_IP, PORT);

    if (!check_server_alive()) {
        printf("Сервер недоступен. Запустите server.c и повторите.\n");
        return -1;
    }

    do {
        memset(packet, 0, PACKET_SIZE);
        int ch; while ((ch = getchar()) != '\n' && ch != EOF);

        get_user_input(packet);

        uint16_t crc = crc16(packet, 10);
        packet[10] = (crc >> 8) & 0xFF;
        packet[11] = crc & 0xFF;

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { perror("socket"); return -1; }
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("connect"); close(sock); continue;
        }

        if (send(sock, packet, PACKET_SIZE, 0) < 0) { perror("send"); close(sock); continue; }

        uint8_t resp_buf[64];
        ssize_t n = read(sock, resp_buf, sizeof(resp_buf));
        if (n >= 4) {
            uint32_t resp = (resp_buf[0] << 24) | (resp_buf[1] << 16) |
                            (resp_buf[2] << 8) | resp_buf[3];
            uint16_t vmax = (packet[7] << 8) | packet[8];
            uint8_t digits = packet[9];
            decode_response(resp, vmax, digits);
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
