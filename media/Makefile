CC = gcc
CPPFLAGS ?=
CFLAGS ?= -O2 -Wall -Wextra
LDFLAGS ?=
BINDIR := bin
SUPPORT := ../support
AUDIO_TOOLS := foc_aiff_info foc_assemble_cached foc_assemble_manifest foc_audio_qc foc_batch_plan foc_cache_gaps foc_cache_inventory foc_chunk_jsonl foc_chunk_plan foc_duration_report foc_jsonl foc_manifest_check foc_manifest_srt foc_manifest_summary foc_plan foc_png_queue foc_speakers foc_termscan foc_validate foc_wav_info
BOB_TOOLS := bob_fetch_youtube bob_mux bob_vtt100
PNG_TOOLS := foc_prepare foc_imagegen
AUDIO_BINS := $(addprefix $(BINDIR)/,$(AUDIO_TOOLS))
BOB_BINS := $(addprefix $(BINDIR)/,$(BOB_TOOLS))
PNG_BINS := $(addprefix $(BINDIR)/,$(PNG_TOOLS))
OPENSSL_PREFIX ?= /opt/homebrew/opt/openssl@3

.PHONY: all core audio bob png aa clean test test-json foc-status foc-finalize
all: core aa
core: audio bob png
audio: $(AUDIO_BINS)
bob: $(BOB_BINS)
png: $(PNG_BINS)
aa: $(BINDIR)/aa_render_c

$(BINDIR):
	mkdir -p $@

$(BINDIR)/foc_common.o: $(SUPPORT)/foc_common.c $(SUPPORT)/foc_common.h | $(BINDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -pedantic -I$(SUPPORT) -c $< -o $@

$(BINDIR)/cutil.o: $(SUPPORT)/cutil.c $(SUPPORT)/cutil.h | $(BINDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -D_DARWIN_C_SOURCE -I$(SUPPORT) -c $< -o $@

$(AUDIO_BINS): $(BINDIR)/%: %.c $(BINDIR)/foc_common.o $(SUPPORT)/foc_common.h | $(BINDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -pedantic -I$(SUPPORT) $< $(BINDIR)/foc_common.o -o $@ $(LDFLAGS) -lm

$(BOB_BINS) $(PNG_BINS): $(BINDIR)/%: %.c $(BINDIR)/cutil.o $(SUPPORT)/cutil.h | $(BINDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -D_DARWIN_C_SOURCE -I$(SUPPORT) $< $(BINDIR)/cutil.o -o $@ $(LDFLAGS)

$(BINDIR)/aa_render_c: aa_render_c.c $(SUPPORT)/json_compat.c $(SUPPORT)/json_compat.h | $(BINDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c17 -O3 -DNDEBUG -I$(SUPPORT) -I$(OPENSSL_PREFIX)/include $< $(SUPPORT)/json_compat.c -o $@ $(LDFLAGS) -L$(OPENSSL_PREFIX)/lib -lcrypto -lm

$(BINDIR)/json_compat_test: $(SUPPORT)/json_compat_test.c $(SUPPORT)/json_compat.c $(SUPPORT)/json_compat.h | $(BINDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -UNDEBUG -std=c11 -I$(SUPPORT) $(SUPPORT)/json_compat_test.c $(SUPPORT)/json_compat.c -o $@ $(LDFLAGS) -lm

test-json: $(BINDIR)/json_compat_test
	$(BINDIR)/json_compat_test

# Set AA_INVENTORY_DIR to scan real recordings; the default is an empty-directory smoke test.
test: aa test-json
	@inventory_dir="$(AA_INVENTORY_DIR)"; temporary=0; \
	if [ -z "$$inventory_dir" ]; then inventory_dir=$$(mktemp -d) || exit 1; temporary=1; fi; \
	$(BINDIR)/aa_render_c inventory "$$inventory_dir"; result=$$?; \
	if [ "$$temporary" = 1 ]; then rmdir "$$inventory_dir"; fi; exit $$result

clean:
	python3 ../clean.py

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

foc-status: audio
	$(BINDIR)/foc_cache_inventory $(FOC_CACHE)

foc-finalize: audio
	$(BINDIR)/foc_manifest_check $(FOC_PREVIEW_JSON)
	$(BINDIR)/foc_assemble_manifest $(FOC_SCRIPT) $(FOC_PREVIEW_JSON) $(FOC_FINAL_AIFF) $(FOC_FINAL_JSON) $(FOC_LINE_SILENCE) $(FOC_FINAL_SRT) $(FOC_PNG_QUEUE) $(FOC_PNG_WIDTH) $(FOC_PNG_HEIGHT)
	$(BINDIR)/foc_audio_qc $(FOC_FINAL_JSON)
