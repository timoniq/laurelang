LDFLAGS = -L/usr/local/lib -lreadline -lm -I'../../src' -I'../../standard'
CC = gcc

all:
	$(CC) -g -shared -o array.so array.c laurelang.so -fPIC $(LDFLAGS)
