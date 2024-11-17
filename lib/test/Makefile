LDFLAGS = -L/usr/local/lib -lreadline -lm -I'../../src' -I'../../standard' -Wno-incompatible-function-pointer-types
CC = gcc

all:
	$(CC) -fPIC -g -shared -o test.so test.c laurelang.so $(LDFLAGS)
