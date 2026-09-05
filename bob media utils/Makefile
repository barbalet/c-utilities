CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -D_DARWIN_C_SOURCE
LDFLAGS ?=

BIN := bob_fetch_youtube bob_mux bob_vtt100
COMMON_SRC := ../common/cutil.c
COMMON_HDR := ../common/cutil.h

.PHONY: all clean

all: $(BIN)

bob_fetch_youtube: bob_fetch_youtube.c $(COMMON_SRC) $(COMMON_HDR)
	$(CC) $(CFLAGS) -I../common -o $@ bob_fetch_youtube.c $(COMMON_SRC) $(LDFLAGS)

bob_mux: bob_mux.c $(COMMON_SRC) $(COMMON_HDR)
	$(CC) $(CFLAGS) -I../common -o $@ bob_mux.c $(COMMON_SRC) $(LDFLAGS)

bob_vtt100: bob_vtt100.c $(COMMON_SRC) $(COMMON_HDR)
	$(CC) $(CFLAGS) -I../common -o $@ bob_vtt100.c $(COMMON_SRC) $(LDFLAGS)

clean:
	rm -f $(BIN)
