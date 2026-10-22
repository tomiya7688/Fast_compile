CC ?= cc
PYTHON ?= python3
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS ?=
LDFLAGS ?=

APP_SOURCES = src/main.c src/driver.c src/source.c
APP_HEADERS = src/driver.h src/source.h

.PHONY: all test clean
all: build/fastc

build:
	mkdir -p build

build/fastc: $(APP_SOURCES) $(APP_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc $(APP_SOURCES) $(LDFLAGS) -o $@

build/test_source: tests/test_source.c src/source.c src/source.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc tests/test_source.c src/source.c $(LDFLAGS) -o $@

test: build/fastc build/test_source
	./build/test_source
	$(PYTHON) -m unittest discover -s tests -p 'test_cli.py' -v

clean:
	rm -rf build
