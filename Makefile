TARGET = laure
SOURCES = src
PREFIX = /usr/local
CFLAGS = -I$(SOURCES) -I/usr/local/include -g $(LIBFLAG) -fPIC -rdynamic
LDFLAGS = -L/usr/local/lib -lreadline -lm -g -ldl

.PHONY: all clean

LIB = $(SOURCES)/parser.o $(SOURCES)/string.o $(SOURCES)/instance.o $(SOURCES)/query.o $(SOURCES)/session.o $(SOURCES)/scope.o $(SOURCES)/domain.o $(SOURCES)/bigint.o $(SOURCES)/builtin.o $(SOURCES)/predicates.o  $(SOURCES)/apply.o
OBJECTS = laure.o $(LIB)

ifeq ($(DEBUG), true)
	CFLAGS := $(CFLAGS) -DDEBUG=1
else
endif

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -ldl -rdynamic -g -o $(TARGET) $(OBJECTS) $(LDFLAGS)

packages: $(LIB)
	$(CC) -fPIC -shared -g -o laurelang.so $(LIB) $(LDFLAGS)
	python3 utility/build_pkg.py

clean:
	rm -f $(TARGET) *.o $(SOURCES)/*.o *.so lib/*.so lib/*/*.so lib/*/src/*.o
	find . -name "*.dSYM" -prune -exec rm -rf {} \;

test:
	./$(TARGET) @/test tests -q "tests_run()" -norepl -signal

install:
	install $(TARGET) $(PREFIX)/bin

help:
	@cat MAKEHELP

src/string.o: src/laurelang.h src/string.c
src/parser.o: src/laurelang.h src/parser.c
src/scope.o: src/laurelang.h src/scope.c
src/session.o: src/laurelang.h src/session.c
src/instance.o: src/laurelang.h src/laureimage.h src/instance.c
src/bigint.o: src/bigint.h src/bigint.c
src/domain.o: src/domain.h src/domain.c
src/query.o: src/laurelang.h src/laureimage.h src/query.c
src/builtin.o: src/laurelang.h src/laureimage.h src/builtin.h src/builtin.c
src/predicates.o: src/laurelang.h src/laureimage.h src/predicates.h src/predicates.c
src/apply.o: src/laurelang.h src/apply.c