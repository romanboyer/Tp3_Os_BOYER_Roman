CC = cc
CFLAGS = -Wall -Werror -pthread

default: biceps

biceps: triceps.c creme.o
	$(CC) $(CFLAGS) -o biceps triceps.c creme.o

memory-leak: CFLAGS = -Wall -Werror -pthread -g -O0
memory-leak: triceps.c creme.o
	$(CC) $(CFLAGS) -o biceps-memory-leaks triceps.c creme.o

creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -c creme.c

clean:
	rm -f biceps biceps-memory-leaks *.o
