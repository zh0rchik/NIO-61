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

#define PORT 8080          // порт сервера
#define PACKET_SIZE 12     // размер входного пакета
#define MAX_CLIENTS 10     // максимальное число клиентов в очереди

// Формирование 32-битного ответа по протоколу
uint32_t make_response(int16_t value, uint16_t vmax, uint8_t digits, uint16_t addr) {
    uint32_t resp = 0;
    resp |= (1u << 31); // бит C = 1 (ответ действителен)

    int is_negative = (value < 0);   // проверка на знак
    int abs_value = abs(value);      // модуль значения

    if (digits == 0 || digits > 15) return 0; // ограничение по числу разрядов
    addr &= 0x1FFF; // используем только младшие 13 бит адреса

    // Вычисляем веса разрядов (делим Vmax пополам каждый раз)
    uint32_t weights[16];
    uint32_t w = vmax;
    for (uint8_t i = 0; i < digits; i++) {
        weights[i] = w;
        w /= 2;
    }

    // Формируем "шаблон" включённых разрядов
    uint32_t pattern = 0;
    int remaining = abs_value;
    for (uint8_t i = 0; i < digits; i++) {
        if (remaining >= (int)weights[i]) {
            pattern |= (1u << (digits - 1 - i));
            remaining -= weights[i];
        }
    }

    // Если значение отрицательное → переводим pattern в дополнительный код
    if (is_negative) {
        uint32_t mask = (digits >= 32) ? 0xFFFFFFFFu : ((1u << digits) - 1u);
        pattern = ((~pattern) + 1u) & mask;
        resp |= (1u << 28); // бит S = 1 (отрицательное число)
    }

    // Переносим pattern в ответ (биты 27..)
    for (uint8_t i = 0; i < digits; i++) {
        if ((pattern >> (digits - 1 - i)) & 1u) {
            resp |= (1u << (27 - i));
        }
    }

    // В младшие 13 бит кладём адрес
    resp |= (addr & 0x1FFFu);
    return resp;
}

// Поток для обработки клиента
void *handle_client(void *arg) {
    int client_fd = *(int *)arg; // дескриптор клиента
    free(arg);                   // освобождаем память под него

    uint8_t buffer[PACKET_SIZE]; // буфер для входного пакета
    ssize_t bytes_read = read(client_fd, buffer, PACKET_SIZE);

    if (bytes_read == PACKET_SIZE) {
        // проверяем CRC16
        uint16_t recv_crc = (buffer[10] << 8) | buffer[11];
        uint16_t calc_crc = crc16(buffer, 10);

        if (recv_crc == calc_crc) {
            // Разбираем поля пакета
            uint16_t client_id = (buffer[0] << 8) | buffer[1];  // ID клиента
            uint16_t addr = (buffer[2] << 6) | (buffer[3] << 3) | buffer[4]; // адрес (13 бит)
            int16_t value = (int16_t)((buffer[5] << 8) | buffer[6]);         // измеренное значение
            uint16_t vmax = (buffer[7] << 8) | buffer[8];                    // максимальное значение
            uint8_t digits = buffer[9];                                      // число разрядов

            // Проверяем входные данные
            if (digits == 0 || digits > 27 || vmax == 0) {
                const char *msg = "ERROR: invalid request";
                send(client_fd, msg, strlen(msg), 0);
            } else {
                // Формируем ответ
                uint32_t resp = make_response(value, vmax, digits, addr);

                // Отправляем клиенту 4 байта ответа
                uint8_t out[4];
                out[0] = (resp >> 24) & 0xFF;
                out[1] = (resp >> 16) & 0xFF;
                out[2] = (resp >> 8) & 0xFF;
                out[3] = resp & 0xFF;
                send(client_fd, out, 4, 0);

                // Печатаем строку в таблицу (лог сервера)
                printf("%-8u %-7o %-6d %-6u %-6u 0x%08X\n",
                       client_id, addr, value, vmax, digits, resp);
            }
        } else {
            // CRC не совпал
            const char *msg = "ERROR: invalid CRC16";
            send(client_fd, msg, strlen(msg), 0);
        }
    } else {
        // пакет пришёл не того размера
        const char *msg = "ERROR: invalid packet size";
        send(client_fd, msg, strlen(msg), 0);
    }

    close(client_fd); // закрываем соединение
    return NULL;
}

int main(void) {
    int server_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    // создаём TCP-сокет
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // разрешаем переиспользование порта
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // настраиваем адрес
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // слушаем все интерфейсы
    address.sin_port = htons(PORT);       // задаём порт

    // привязываем сокет к адресу
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // переводим сокет в режим прослушивания
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Заголовок таблицы для вывода логов
    printf("TCP Server listening on port %d\n", PORT);
    printf("%-8s %-7s %-6s %-6s %-6s %s\n", "ID", "Adress", "Value", "V_MAX", "DIGIT", "Response");

    // Бесконечный цикл приёма клиентов
    while (1) {
        int *client_fd = malloc(sizeof(int)); // выделяем память под fd клиента
        if (!client_fd) continue;

        // ждём подключения
        *client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (*client_fd < 0) {
            free(client_fd);
            continue;
        }

        // создаём поток для обработки клиента
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_fd) != 0) {
            close(*client_fd);
            free(client_fd);
            continue;
        }
        pthread_detach(tid); // поток не нужно join-ить
    }

    close(server_fd); // закрываем серверный сокет
    return 0;
}
