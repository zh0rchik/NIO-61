#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "crc16.h"

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define PACKET_SIZE 12

// // Структура запроса
// typedef struct {
//     uint16_t client_id;      // байты 0–1
//     uint8_t  addr[3];        // байты 2–4 (адрес в 8-ричной форме)
//     int16_t  value;          // байты 5–6 (число со знаком)
//     uint16_t vmax;           // байты 7–8 (цена старшего разряда)
//     uint8_t  digits;         // байт 9 (количество разрядов)
//     uint16_t crc;            // байты 10–11 (контрольная сумма CRC16)
// } Request;

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    uint8_t packet[PACKET_SIZE];

    // 1. ID клиента (123 = 0x007B)
    packet[0] = 0x00;
    packet[1] = 0x7B;

    // 2. Адрес (075 в восьмеричном виде: "0", "7", "5")
    packet[2] = 0;
    packet[3] = 7;
    packet[4] = 5;

    // 3. Число (-25 в big endian: 0xFFE7)
    packet[5] = 0xFF;
    packet[6] = 0xE7;

    // 4. Цена старшего разряда (1000 = 0x03E8 → big endian)
    packet[7] = 0x03;
    packet[8] = 0xE8;

    // 5. Количество разрядов
    packet[9] = 5;

    // 6. Считаем CRC16 по первым 10 байтам
    uint16_t crc = crc16(packet, 10);
    packet[10] = (crc >> 8) & 0xFF; // старший байт
    packet[11] = crc & 0xFF;        // младший байт

    // --- СЕТЬ ---
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

    send(sock, packet, PACKET_SIZE, 0);
    printf("Отправлен пакет с контрольной суммой (CRC16) = %X\n", crc);

    close(sock);
    return 0;
}