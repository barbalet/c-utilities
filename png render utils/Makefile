CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -D_DARWIN_C_SOURCE
LDFLAGS ?=

BIN := foc_prepare

.PHONY: all clean

all: $(BIN)

$(BIN): foc_prepare.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BIN)
