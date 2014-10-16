# mimimal Makefile

all: server client

CFLAGS = -g

server : server.c
client : client.c

clean: 
	rm server client
