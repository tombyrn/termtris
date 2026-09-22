CC = clang
CFLAGS  = -g -Wall -Werror -std=c17 -lpthread

all: tetris

tetris: server.o client.o tetris.c
	$(CC) tetris.c server.o client.o $(CFLAGS) -o tetris

server.o: server.c
	$(CC) -c server.c

client.o: client.c
	$(CC) -c client.c

clean:
	rm -rf tetris tetris.dSYM *.o