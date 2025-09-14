#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 12345
#define SERVER_IP "127.0.0.1"
#define PACKET_SIZE 12

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    uint8_t packet[PACKET_SIZE] = {
        0x00, 0x7B,   // ID клиента = 123
        0x00, 0x07, 0x05, // адрес (075 в восьмеричном виде)
        0xFF, 0xE7,   // число = -25 (в big endian)
        0x03, 0xE8,   // цена старшего разряда = 1000
        0x05,         // количество разрядов = 5
        0x12, 0x34    // CRC (пока заглушка, можно потом посчитать правильно)
    };

    // 1. Создаём сокет
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 2. Преобразуем IP адрес в бинарную форму
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    // 3. Подключаемся к серверу
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    // 4. Отправляем пакет
    send(sock, packet, PACKET_SIZE, 0);
    printf("Packet sent to server!\n");

    // 5. Закрываем соединение
    close(sock);

    return 0;
}
