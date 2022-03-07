TARGET = laure
PREFIX = /usr/local
LIBFLAG = -Dlib_path=\"lib\"
CFLAGS = -Isrc -Isubmodules/bigint -I/usr/local/include -g $(LIBFLAG) -fPIE -rdynamic
LDFLAGS = -L/usr/local/lib -lreadline -lm -g
CC = gcc

ifeq ($(DEBUG), 1)
	CFLAGS := $(CFLAGS) -DDEBUG=1
endif

ifeq ($(FEATURE_LINKED_SCOPE), 1)
    CFLAGS := $(CFLAGS) -DFEATURE_LINKED_SCOPE=1
endif

.PHONY: all clean

LIB = src/string.o src/parser.o src/session.o src/stack.o src/instance.o src/bigint.o src/domain.o src/query.o src/builtin.o src/predicates.o src/apply.o src/gc.o

OBJECTS = laure.o $(LIB)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -ldl -rdynamic -g -o $(TARGET) $(OBJECTS) $(LDFLAGS)

packages: $(LIB)
	$(CC) -fPIE -shared -g -o laurelang.so $(LIB) $(LDFLAGS)
	python3 utility/build_pkg.py

clean:
	rm -f $(TARGET) *.o src/*.o *.so lib/*.so lib/*/*.so lib/*/src/*.o
	find . -name "*.dSYM" -prune -exec rm -rf {} \;

auto:
	make LIBFLAG=-Dlib_path=\"\\\"$(PREFIX)/opt/laurelang/lib\\\"\"
	make packages
	echo "Linking standard library"
	mkdir -p $(PREFIX)/opt/laurelang
	cp -r ./lib $(PREFIX)/opt/laurelang
	make install
	echo "Done"

test:
	LLTIMEOUT=3 ./laure @/test tests -q "tests_run()" -norepl -signal

install:
	install $(TARGET) $(PREFIX)/bin

uninstall:
	rm -rf $(PREFIX)/bin/$(TARGET)

info:
	git ls-files | xargs cat | wc -l

src/string.o: src/laurelang.h src/string.c
src/parser.o: src/laurelang.h src/parser.c
src/stack.o: src/laurelang.h src/stack.c
src/session.o: src/laurelang.h src/session.c
src/instance.o: src/laurelang.h src/laureimage.h src/instance.c
src/bigint.o: src/bigint.h src/bigint.c
src/domain.o: src/domain.h src/domain.c
src/query.o: src/laurelang.h src/laureimage.h src/query.c
src/builtin.o: src/laurelang.h src/laureimage.h src/builtin.h src/builtin.c
src/predicates.o: src/laurelang.h src/predicates.h src/predicates.c
src/apply.o: src/laurelang.h src/predpub.h src/apply.c
src/gc.o: src/laurelang.h src/laureimage.h src/gc.c
