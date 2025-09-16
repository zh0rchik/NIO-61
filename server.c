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

// === Формирование 32-битного ответа ===
uint32_t make_response(int16_t value, uint16_t vmax, uint8_t digits, uint16_t addr) {
    uint32_t resp = 0;
    resp |= (1u << 31);  // C = 1

    if (value < 0) {
        resp |= (1u << 28);  // S = 1
        value = -value;
    }

    double step = (double)vmax;
    for (int i = 0; i < digits; i++) {
        if (value >= (int)step) {
            resp |= (1u << (27 - i));
            value -= (int)step;
        }
        step /= 2.0;
    }

    resp |= (addr & 0x1FFF); // младшие 13 бит = адрес
    return resp;
}

// === Обработка клиента в отдельном потоке ===
void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    uint8_t buffer[PACKET_SIZE];
    int bytes_read = read(client_fd, buffer, PACKET_SIZE);

    if (bytes_read == PACKET_SIZE) {
        uint16_t recv_crc = (buffer[10] << 8) | buffer[11];
        uint16_t calc_crc = crc16(buffer, 10);

        if (recv_crc == calc_crc) {
            uint16_t client_id = (buffer[0] << 8) | buffer[1];
            uint16_t addr = (buffer[2] << 8) | (buffer[3] << 4) | buffer[4];
            int16_t value = (int16_t)((buffer[5] << 8) | buffer[6]);
            uint16_t vmax = (buffer[7] << 8) | buffer[8];
            uint8_t digits = buffer[9];

            printf("[Client %u] addr=%u, value=%d, vmax=%u, digits=%u\n",
                   client_id, addr, value, vmax, digits);

            uint32_t resp = make_response(value, vmax, digits, addr);

            uint8_t out[4];
            out[0] = (resp >> 24) & 0xFF;
            out[1] = (resp >> 16) & 0xFF;
            out[2] = (resp >> 8) & 0xFF;
            out[3] = resp & 0xFF;

            send(client_fd, out, 4, 0);
        } else {
            char *msg = "invalid request";
            send(client_fd, msg, strlen(msg), 0);
        }
    }

    close(client_fd);
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
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

    printf("Server is listening on port %d...\n", PORT);

    while (1) {
        int *client_fd = malloc(sizeof(int));
        if (!client_fd) continue;

        *client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (*client_fd < 0) {
            perror("accept failed");
            free(client_fd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
