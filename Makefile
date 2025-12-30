CC = gcc
CFLAGS = -Wall -g -I/usr/local/include
LDFLAGS = -L/usr/local/lib
LIBS = -lraylib -lm -lpthread -ldl -lrt -lX11

main: main.c
	$(CC) $(CFLAGS) main.c -o main $(LDFLAGS) $(LIBS)

