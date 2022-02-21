LDFLAGS = -L/usr/local/lib -lreadline -lm -I'../../src'
CC = gcc

all:
	$(CC) -g -shared -o test.so test.c laurelang.so -fPIC $(LDFLAGS)
