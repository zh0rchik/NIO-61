#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "crc16.h"

#define PORT 8080
#define PACKET_SIZE 12

// Формирование 32-битного ответа
uint32_t make_response(int16_t value, uint16_t vmax, uint8_t digits, uint16_t addr) {
    uint32_t resp = 0;

    // 1. Ставим C = 1 (бит 31)
    resp |= (1u << 31);

    // 2. Определяем знак
    if (value < 0) {
        resp |= (1u << 28);  // S = 1
        value = -value;      // работаем с модулем
    }

    // 3. Заполняем digits битов начиная с 27
    double step = (double)vmax;
    for (int i = 0; i < digits; i++) {
        if (value >= (int)step) {
            resp |= (1u << (27 - i));
            value -= (int)step;
        }
        step /= 2.0;
    }

    // 4. Вставляем адрес (13 младших бит)
    resp |= (addr & 0x1FFF);

    return resp;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    uint8_t buffer[PACKET_SIZE];

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Порт %d...\n", PORT);

    if ((client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
        perror("accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected\n");

    int bytes_read = read(client_fd, buffer, PACKET_SIZE);
    if (bytes_read == PACKET_SIZE) {
        printf("Received packet:\n");
        for (int i = 0; i < PACKET_SIZE; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");

        // Проверка CRC
        uint16_t recv_crc = (buffer[10] << 8) | buffer[11];
        uint16_t calc_crc = crc16(buffer, 10);

        if (recv_crc == calc_crc) {
            printf("CRC OK! (0x%04X)\n", recv_crc);

            // Извлекаем данные
            uint16_t client_id = (buffer[0] << 8) | buffer[1];
            uint16_t addr = (buffer[2] << 8) | (buffer[3] << 4) | buffer[4]; // из 3х восьмеричных цифр
            int16_t value = (int16_t)((buffer[5] << 8) | buffer[6]);
            uint16_t vmax = (buffer[7] << 8) | buffer[8];
            uint8_t digits = buffer[9];

            printf("Parsed: ID=%u, addr=%u, value=%d, vmax=%u, digits=%u\n",
                   client_id, addr, value, vmax, digits);

            // Формируем ответ
            uint32_t resp = make_response(value, vmax, digits, addr);

            // Отправляем 4 байта (big endian)
            uint8_t out[4];
            out[0] = (resp >> 24) & 0xFF;
            out[1] = (resp >> 16) & 0xFF;
            out[2] = (resp >> 8) & 0xFF;
            out[3] = resp & 0xFF;

            send(client_fd, out, 4, 0);
            printf("Response sent: 0x%08X\n", resp);

        } else {
            printf("CRC ERROR! Received=0x%04X, Calculated=0x%04X\n", recv_crc, calc_crc);
            char *msg = "invalid request";
            send(client_fd, msg, strlen(msg), 0);
        }
    }

    close(client_fd);
    close(server_fd);
    return 0;
}
