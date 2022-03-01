LDFLAGS = -L/usr/local/lib -lreadline -lm -I'../../src'
CFLAGS = -I'../../src' -g -fPIC
CC = gcc

OBJECTS = src/server.o src/serve.o src/send.o src/event.o

all: $(OBJECTS)
	$(CC) -g -shared -fPIC -o server.so $(OBJECTS) laurelang.so $(LDFLAGS)

src/server.o: src/server.h src/server.c
src/serve.o: src/server.h src/serve.c
src/send.o: src/server.h src/send.c
src/event.o: src/server.h src/event.c
