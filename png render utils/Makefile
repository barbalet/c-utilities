CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -D_DARWIN_C_SOURCE
LDFLAGS ?=

BIN := foc_prepare foc_imagegen
COMMON_SRC := ../common/cutil.c
COMMON_HDR := ../common/cutil.h

.PHONY: all clean

all: $(BIN)

foc_prepare: foc_prepare.c $(COMMON_SRC) $(COMMON_HDR)
	$(CC) $(CFLAGS) -I../common -o $@ foc_prepare.c $(COMMON_SRC) $(LDFLAGS)

foc_imagegen: foc_imagegen.c $(COMMON_SRC) $(COMMON_HDR)
	$(CC) $(CFLAGS) -I../common -o $@ foc_imagegen.c $(COMMON_SRC) $(LDFLAGS)

clean:
	rm -f $(BIN)
