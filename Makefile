TARGET = laure
SOURCES = src
PREFIX = /usr/local
WS_FLAGS = 
CFLAGS = -Isrc -Icompiler -I/usr/local/include -g $(LIBFLAG) -fPIC ${ADDFLAGS} ${WS_FLAGS} -Wno-incompatible-function-pointer-types -Wno-incompatible-pointer-types-discards-qualifiers
LDFLAGS = -L/usr/local/lib -lreadline -lm -g -ldl

.PHONY: all clean

LIB = src/parser.o \
	  src/string.o \
	  src/instance.o \
	  src/query.o \
	  src/session.o \
	  src/scope.o \
	  src/domain.o \
	  src/bigint.o \
	  src/builtin.o \
	  src/predicates.o  \
	  src/apply.o \
	  src/error.o \
	  src/backtrace.o \
	  compiler/compiler.o \
	  compiler/cli.o \
	  compiler/runtime.o \
	  src/weight.o

OBJECTS = laure.o $(LIB)

ifeq ($(DEBUG), true)
	CFLAGS := $(CFLAGS) -DDEBUG=1
	WS_FLAGS := $(WS_FLAGS) -DDEBUG=1
else
endif

ifeq ($(NOWS), true)
	CFLAGS := $(CFLAGS) -DDISABLE_WS=1
	LDFLAGS := $(LDFLAGS) -DDISABLE_WS=1
	WS_FLAGS := $(WS_FLAGS) -DDISABLE_WS=1
else
endif

all: $(TARGET)

compiler/$(fname).o: compiler/compiler.h src/laurelang.h compiler/$(fname).c

$(TARGET): $(OBJECTS)
	@echo Linking
	$(CC) -ldl -g -o $(TARGET) $(OBJECTS) $(LDFLAGS)

packages: $(LIB)
	$(CC) -fPIC -shared -g -o laurelang.so $(LIB) $(LDFLAGS)
	python3 utility/build_pkg.py

clean:
	rm -f $(TARGET) *.o src/*.o src/*.so *.so lib/*.so lib/*/*.so lib/*/src/*.o compiler/*.o
	find . -name "*.dSYM" -prune -exec rm -rf {} \;

test:
	./$(TARGET) @/test tests \
	-q "tests_run()" \
	-D skip="test_pred_.*;test_nested_2" \
	-norepl -signal --ignore

auto:
	@make clean
	@make
	@make packages
	@make test

install: $(TARGET)
	mkdir -p $(PREFIX)/bin
	cp -f $(TARGET) $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

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
src/error.o: src/laurelang.h src/error.c
src/backtrace.o: src/laurelang.h src/backtrace.c
src/weight.o: src/laurelang.h src/weight.c