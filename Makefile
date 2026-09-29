CC = gcc
CFLAGS = -Wall -Wextra -std=c17 -lpthread

planificador: main.c
	$(CC) $(CFLAGS) -o planificador main.c

clean:
	rm -f planificador

