CFLAGS = -w
CC = gcc

all: client server

client: client.o utils.o player.o board.o game.o
	${CC} ${CFLAGS} -o client client.o utils.o player.o board.o game.o -lpthread -lncurses

server: server.o utils.o player.o board.o game.o
	${CC} ${CFLAGS} -o server server.o utils.o player.o board.o game.o -lpthread

client.o: client.c structs.h utils.h player.h board.h game.h
	${CC} ${CFLAGS} -c client.c

server.o: server.c structs.h
	${CC} ${CFLAGS} -c server.c

utils.o: utils.c structs.h
	${CC} ${CFLAGS} -c utils.c

game.o: game.c structs.h utils.h player.h board.h
	${CC} ${CFLAGS} -c game.c

player.o: player.c structs.h
	${CC} ${CFLAGS} -c player.c

board.o: board.c structs.h
	${CC} ${CFLAGS} -c board.c

clean:
	rm -f *.o *~ server client