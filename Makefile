CC = gcc
CFLAGS = -Wall -Wextra -O2

all: server client

server: server.c crc16.c crc16.h
	$(CC) $(CFLAGS) server.c crc16.c -o server

client: client.c crc16.c crc16.h
	$(CC) $(CFLAGS) client.c crc16.c -o client

clean:
	rm -f server client *.o
