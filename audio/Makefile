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
	$(BINDIR)/foc_assemble_cached \
	$(BINDIR)/foc_assemble_manifest \
	$(BINDIR)/foc_chunk_plan \
	$(BINDIR)/foc_chunk_jsonl \
	$(BINDIR)/foc_manifest_check \
	$(BINDIR)/foc_duration_report \
	$(BINDIR)/foc_cache_gaps \
	$(BINDIR)/foc_manifest_summary \
	$(BINDIR)/foc_manifest_srt \
	$(BINDIR)/foc_png_queue \
	$(BINDIR)/foc_batch_plan \
	$(BINDIR)/foc_audio_qc \
	$(BINDIR)/foc_cache_inventory

FOC_SCRIPT ?= text/foc_script_txt/foc_script.txt
FOC_PREVIEW_JSON ?= text/foc_script_txt/foc_script.python-preview.json
FOC_FINAL_AIFF ?= text/foc_script_txt/foc_script.aiff
FOC_FINAL_JSON ?= text/foc_script_txt/foc_script.json
FOC_FINAL_SRT ?= text/foc_script_txt/foc_script.srt
FOC_PNG_QUEUE ?= text/foc_script_txt/foc_script_png_queue.jsonl
FOC_CACHE ?= .cache/chatterbox/foc_script_segments
FOC_LINE_SILENCE ?= 0.18
FOC_PNG_WIDTH ?= 1080
FOC_PNG_HEIGHT ?= 1920

.PHONY: all clean foc-status foc-finalize

all: $(TOOLS)

$(BINDIR):
	mkdir -p $(BINDIR)

foc_common.o: foc_common.c foc_common.h
	$(CC) $(CFLAGS) -c foc_common.c -o $@

$(BINDIR)/%: %.c $(COMMON) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(COMMON) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BINDIR) *.o

foc-status: all
	$(BINDIR)/foc_cache_inventory $(FOC_CACHE)

foc-finalize: all
	$(BINDIR)/foc_manifest_check $(FOC_PREVIEW_JSON)
	$(BINDIR)/foc_assemble_manifest $(FOC_SCRIPT) $(FOC_PREVIEW_JSON) $(FOC_FINAL_AIFF) $(FOC_FINAL_JSON) $(FOC_LINE_SILENCE) $(FOC_FINAL_SRT) $(FOC_PNG_QUEUE) $(FOC_PNG_WIDTH) $(FOC_PNG_HEIGHT)
	$(BINDIR)/foc_audio_qc $(FOC_FINAL_JSON)
