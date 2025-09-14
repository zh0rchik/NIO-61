#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "crc16.h"

#define PORT 8080   // порт сервера
#define PACKET_SIZE 12

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    uint8_t buffer[PACKET_SIZE];

    // 1. Создаём TCP-сокет
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 2. Привязываем сокет к порту
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // слушаем все интерфейсы
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Ставим сервер в режим ожидания подключений
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    // 4. Принятие одного соединение
    if ((client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
        perror("accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected!\n");

    // 5. Считываение 12 байтов
    int bytes_read = read(client_fd, buffer, PACKET_SIZE);
    
    printf("Отправленный пакет:\n");
    for (int i = 0; i < PACKET_SIZE; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n");

    // Проверка CRC
    uint16_t recv_crc = (buffer[10] << 8) | buffer[11];
    uint16_t calc_crc = crc16(buffer, 10);

    if (recv_crc == calc_crc) {
        printf("CRC OK! valid request\n(0x%04X)\n", recv_crc);
    } else {
        printf("invalid request\nReceived=0x%04X, Calculated=0x%04X\n", recv_crc, calc_crc);
    }

    // 6. Закрываем соединение
    close(client_fd);
    close(server_fd);

    return 0;
}
