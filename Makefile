CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread

all: server client

server: server.c crc16.c crc16.h
	$(CC) $(CFLAGS) server.c crc16.c -o server

client: client.c crc16.c crc16.h
	$(CC) $(CFLAGS) client.c crc16.c -lm -o client

clean:
	rm -f server client *.o
