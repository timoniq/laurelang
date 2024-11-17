TARGET = laure
PREFIX = /usr/local
WS_FLAGS = 
GIT_VER = "$(shell git describe --always --tags)"

CFLAGS = -Isrc \
		 -Istd -Icompiler -I/usr/local/include \
		 -g $(LIBFLAG) -fPIC \
		 ${ADDFLAGS} ${WS_FLAGS} \
		 -Wno-incompatible-function-pointer-types \
		 -Wno-incompatible-pointer-types-discards-qualifiers \
		 -DGIT_VER='$(GIT_VER)'

LDFLAGS = -L/usr/local/lib -Isrc -lreadline -lm -g -ldl -DGIT_VER='$(GIT_VER)'

ifeq ($(UNAME), Linux)
	LDFLAGS := $(LDFLAGS) -luuid
	CFLAGS := $(CFLAGS) -luuid
endif

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
	  src/apply.o \
	  src/error.o \
	  src/backtrace.o \
	  src/weight.o \
	  src/order.o \
	  src/alloc.o \
	  src/pprint.o \
	  src/import.o \
	  std/integer.c \
	  std/utility.c \
	  std/bag.c \
	  std/array.c \
	  std/map.c \
	  std/union.c

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

shared: $(LIB)
	$(CC) -fPIC -shared -g -o laurelang.so $(LIB) $(LDFLAGS)

api: shared
	$(CC) -fPIC -shared -ldl -o laure_api.so api/c/api.c laurelang.so $(LDFLAGS)

packages: shared
	python3 utility/build_pkg.py

clean:
	rm -f $(TARGET) *.o src/*.o src/*.so *.so lib/*.so std_predicate/*.o lib/*/*.so lib/*/src/*.o compiler/*.o
	find . -name "*.dSYM" -prune -exec rm -rf {} \;

package: shared
	python3 utility/build_pkg.py $(name)

test:
	./$(TARGET) "<test>" tests \
	-q "tests_run()" \
	-D skip="test_pred_.*;test_nested_2" \
	--norepl --signal --ignore

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
src/apply.o: src/laurelang.h src/apply.c
src/error.o: src/laurelang.h src/error.c
src/backtrace.o: src/laurelang.h src/backtrace.c
src/weight.o: src/laurelang.h src/weight.c
src/order.o: src/laurelang.h src/laureimage.h src/order.c
src/alloc.o: src/laurelang.h src/alloc.c
src/pprint.o: src/laurelang.h src/pprint.c
src/import.o: src/laurelang.h src/import.c
