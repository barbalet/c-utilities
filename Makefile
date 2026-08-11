CC = gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -pedantic
LDFLAGS ?= -lm
BINDIR := bin
COMMON := foc_common.o

TOOLS := \
	$(BINDIR)/foc_validate \
	$(BINDIR)/foc_speakers \
	$(BINDIR)/foc_termscan \
	$(BINDIR)/foc_jsonl \
	$(BINDIR)/foc_plan \
	$(BINDIR)/foc_wav_info \
	$(BINDIR)/foc_aiff_info \
	$(BINDIR)/foc_assemble_cached

.PHONY: all clean

all: $(TOOLS)

$(BINDIR):
	mkdir -p $(BINDIR)

foc_common.o: foc_common.c foc_common.h
	$(CC) $(CFLAGS) -c foc_common.c -o $@

$(BINDIR)/%: %.c $(COMMON) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(COMMON) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BINDIR) *.o
