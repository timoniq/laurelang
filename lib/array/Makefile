LDFLAGS = -L/usr/local/lib -lm -I'../../src'
CC = gcc

all:
	$(CC) -g -shared -o array.so array.c laurelang.so -fPIC $(LDFLAGS)
