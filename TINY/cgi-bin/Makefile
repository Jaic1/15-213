CC = gcc
CFLAGS = -O2 -Wall -I ..

LIB = -lpthread

all: adder

adder: adder.c ../csapp.o
	$(CC) $(CFLAGS) -o adder adder.c ../csapp.o $(LIB)

clean:
	rm -f *.o adder *~
