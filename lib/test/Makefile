LDFLAGS = -L/usr/local/lib -lreadline -lm -I'../../src'
CC = gcc

all:
	$(CC) -fPIC -g -shared -o test.so test.c ../../laurelang.so $(LDFLAGS)
