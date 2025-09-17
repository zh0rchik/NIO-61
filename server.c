/*
 server.c
 TCP сервер: принимает 12-байтные запросы, проверяет CRC16 и формирует 32-битный ответ.
 Вывод в виде таблицы: ID, Adress, Value, V_MAX, DIGIT, Response HEX.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "crc16.h"

#define PORT 8080
#define PACKET_SIZE 12
#define MAX_CLIENTS 10

uint32_t make_response(int16_t value, uint16_t vmax, uint8_t digits, uint16_t addr) {
    uint32_t resp = 0;
    resp |= (1u << 31); // C = 1

    int is_negative = (value < 0);
    int abs_value = abs(value);

    if (digits == 0 || digits > 15) return 0;
    addr &= 0x1FFF;

    uint32_t weights[16];
    uint32_t w = vmax;
    for (uint8_t i = 0; i < digits; i++) {
        weights[i] = w;
        w /= 2;
    }

    uint32_t pattern = 0;
    int remaining = abs_value;
    for (uint8_t i = 0; i < digits; i++) {
        if (remaining >= (int)weights[i]) {
            pattern |= (1u << (digits - 1 - i));
            remaining -= weights[i];
        }
    }

    if (is_negative) {
        uint32_t mask = (digits >= 32) ? 0xFFFFFFFFu : ((1u << digits) - 1u);
        pattern = ((~pattern) + 1u) & mask;
        resp |= (1u << 28); // S = 1
    }

    for (uint8_t i = 0; i < digits; i++) {
        if ((pattern >> (digits - 1 - i)) & 1u) {
            resp |= (1u << (27 - i));
        }
    }

    resp |= (addr & 0x1FFFu);
    return resp;
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    uint8_t buffer[PACKET_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, PACKET_SIZE);

    if (bytes_read == PACKET_SIZE) {
        uint16_t recv_crc = (buffer[10] << 8) | buffer[11];
        uint16_t calc_crc = crc16(buffer, 10);

        if (recv_crc == calc_crc) {
            uint16_t client_id = (buffer[0] << 8) | buffer[1];
            uint16_t addr = (buffer[2] << 6) | (buffer[3] << 3) | buffer[4];
            int16_t value = (int16_t)((buffer[5] << 8) | buffer[6]);
            uint16_t vmax = (buffer[7] << 8) | buffer[8];
            uint8_t digits = buffer[9];

            if (digits == 0 || digits > 27 || vmax == 0) {
                const char *msg = "ERROR: invalid request";
                send(client_fd, msg, strlen(msg), 0);
            } else {
                uint32_t resp = make_response(value, vmax, digits, addr);
                uint8_t out[4];
                out[0] = (resp >> 24) & 0xFF;
                out[1] = (resp >> 16) & 0xFF;
                out[2] = (resp >> 8) & 0xFF;
                out[3] = resp & 0xFF;
                send(client_fd, out, 4, 0);

                // Табличный вывод
                printf("%-8u %-7o %-6d %-6u %-6u 0x%08X\n",
                       client_id, addr, value, vmax, digits, resp);
            }
        } else {
            const char *msg = "ERROR: invalid CRC16";
            send(client_fd, msg, strlen(msg), 0);
        }
    } else {
        const char *msg = "ERROR: invalid packet size";
        send(client_fd, msg, strlen(msg), 0);
    }

    close(client_fd);
    return NULL;
}

int main(void) {
    int server_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("TCP Server listening on port %d\n", PORT);
    printf("%-8s %-7s %-6s %-6s %-6s %s\n", "ID", "Adress", "Value", "V_MAX", "DIGIT", "Response");

    while (1) {
        int *client_fd = malloc(sizeof(int));
        if (!client_fd) continue;

        *client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (*client_fd < 0) {
            free(client_fd);
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_fd) != 0) {
            close(*client_fd);
            free(client_fd);
            continue;
        }
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
