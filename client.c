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

void decode_response(uint32_t resp, uint16_t vmax, uint8_t digits) {
    printf("\n--- Расшифровка ответа ---\n");
    int C = (resp >> 31) & 1;
    int S = (resp >> 28) & 1;
    uint16_t addr = resp & 0x1FFF;
    printf("C = %d (валидность)\n", C);
    printf("S = %d (знак)\n", S);
    printf("Адрес = %u\n", addr);

    printf("Разряды: ");
    double step = (double)vmax;
    int sum = 0;
    for (int i = 0; i < digits; i++) {
        int bit = (resp >> (27 - i)) & 1;
        printf("%d", bit);
        if (bit) sum += (int)step;
        step /= 2.0;
    }
    printf("\nЗначение по битам = %s%d\n", S ? "-" : "", sum);
    printf("--------------------------\n\n");
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    uint8_t packet[PACKET_SIZE];

    // --- Уникальный ID клиента = PID процесса ---
    uint16_t pid = (uint16_t)getpid();
    packet[0] = (pid >> 8) & 0xFF;
    packet[1] = pid & 0xFF;

    // --- Адрес (075 в восьмеричном виде: цифры "0", "7", "5") ---
    packet[2] = 0;
    packet[3] = 7;
    packet[4] = 5;

    // --- Число (value = -25) ---
    packet[5] = 0xFF;
    packet[6] = 0xE7;

    // --- Цена старшего разряда (1000) ---
    packet[7] = 0x03;
    packet[8] = 0xE8;

    // --- Количество значащих разрядов ---
    packet[9] = 5;

    // --- CRC16 ---
    uint16_t crc = crc16(packet, 10);
    packet[10] = (crc >> 8) & 0xFF;
    packet[11] = crc & 0xFF;

    // --- Сеть ---
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    // --- Отправка ---
    send(sock, packet, PACKET_SIZE, 0);
    printf("Packet sent with CRC16 = %04X (ID = %u)\n", crc, pid);

    // --- Приём ответа ---
    uint8_t resp_buf[4];
    int n = read(sock, resp_buf, 4);
    if (n == 4) {
        uint32_t resp = (resp_buf[0] << 24) | (resp_buf[1] << 16) |
                        (resp_buf[2] << 8) | resp_buf[3];
        printf("Received response: 0x%08X\n", resp);

        decode_response(resp, 1000, 5);
    } else {
        printf("No valid 4-byte response, got %d bytes\n", n);
    }

    close(sock);
    return 0;
}
