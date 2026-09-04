CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -D_DARWIN_C_SOURCE
LDFLAGS ?=

BIN := bob_fetch_youtube bob_mux bob_vtt100

.PHONY: all clean

all: $(BIN)

bob_fetch_youtube: bob_fetch_youtube.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

bob_mux: bob_mux.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

bob_vtt100: bob_vtt100.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BIN)
