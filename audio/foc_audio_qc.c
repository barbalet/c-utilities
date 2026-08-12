#include "foc_common.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int segment_index;
    int source_line;
    const char *path;
    uint64_t frames;
    uint64_t samples;
    uint64_t clipped;
    double sum_squares;
    int peak;
} SegmentAudioStats;

typedef struct {
    int count;
    int bad;
    int silent;
    int clipped_segments;
    int low_peak_segments;
    int expected_rate;
    int expected_channels;
    uint64_t frames;
    uint64_t samples;
    uint64_t clipped;
    double sum_squares;
    int peak;
} QcCtx;

static int sample_abs_i16(int16_t v) {
    return v == INT16_MIN ? 32768 : (v < 0 ? -v : v);
}

static int scan_wav_pcm16(const char *path, SegmentAudioStats *stats, FocWavInfo *info) {
    FILE *in = fopen(path, "rb");
    unsigned char buf[262144];
    uint32_t remaining;
    if (!in) return -1;
    if (foc_wav_info(path, info) != 0 || info->bits_per_sample != 16) {
        fclose(in);
        return -1;
    }
    if (fseek(in, (long)info->data_offset, SEEK_SET) != 0) {
        fclose(in);
        return -1;
    }
    remaining = info->data_bytes;
    while (remaining) {
        size_t want = remaining < sizeof(buf) ? remaining : sizeof(buf);
        size_t got = fread(buf, 1, want, in);
        size_t i;
        if (got == 0) {
            fclose(in);
            return -1;
        }
        for (i = 0; i + 1 < got; i += 2) {
            int16_t sample = (int16_t)((uint16_t)buf[i] | ((uint16_t)buf[i + 1] << 8));
            int a = sample_abs_i16(sample);
            double norm = (double)sample / 32768.0;
            if (a > stats->peak) stats->peak = a;
            if (a >= 32760) stats->clipped++;
            stats->sum_squares += norm * norm;
            stats->samples++;
        }
        remaining -= (uint32_t)got;
    }
    fclose(in);
    stats->frames = info->frames;
    return 0;
}

static double dbfs_from_peak(int peak) {
    if (peak <= 0) return -999.0;
    return 20.0 * log10((double)peak / 32768.0);
}

static double dbfs_from_rms(double sum_squares, uint64_t samples) {
    double rms;
    if (!samples || sum_squares <= 0.0) return -999.0;
    rms = sqrt(sum_squares / (double)samples);
    if (rms <= 0.0) return -999.0;
    return 20.0 * log10(rms);
}

static int qc_segment(const char *object, size_t len, void *raw) {
    QcCtx *ctx = (QcCtx *)raw;
    char *path = foc_json_get_string_slice(object, len, "segment_cache_audio");
    char *speaker = foc_json_get_string_slice(object, len, "speaker");
    SegmentAudioStats stats = {0};
    FocWavInfo info;
    stats.segment_index = (int)foc_json_get_long_slice(object, len, "segment_index", ctx->count + 1);
    stats.source_line = (int)foc_json_get_long_slice(object, len, "source_line", 0);
    stats.path = path ? path : "";

    ctx->count++;
    if (!path || scan_wav_pcm16(path, &stats, &info) != 0) {
        fprintf(stderr, "bad_audio segment=%d line=%d speaker=%s path=%s\n",
                stats.segment_index,
                stats.source_line,
                speaker ? speaker : "",
                path ? path : "");
        ctx->bad++;
        free(path);
        free(speaker);
        return 0;
    }

    if (!ctx->expected_rate) {
        ctx->expected_rate = info.sample_rate;
        ctx->expected_channels = info.channels;
    }
    if (info.sample_rate != ctx->expected_rate || info.channels != ctx->expected_channels) {
        fprintf(stderr, "format_mismatch segment=%d line=%d speaker=%s path=%s\n",
                stats.segment_index, stats.source_line, speaker ? speaker : "", path);
        ctx->bad++;
    }
    if (stats.peak <= 32) {
        fprintf(stderr, "silent_segment segment=%d line=%d speaker=%s peak=%d path=%s\n",
                stats.segment_index, stats.source_line, speaker ? speaker : "", stats.peak, path);
        ctx->silent++;
    } else if (stats.peak < 700) {
        fprintf(stderr, "low_peak_segment segment=%d line=%d speaker=%s peak=%d peak_dbfs=%.1f path=%s\n",
                stats.segment_index, stats.source_line, speaker ? speaker : "", stats.peak, dbfs_from_peak(stats.peak), path);
        ctx->low_peak_segments++;
    }
    if (stats.clipped) {
        fprintf(stderr, "clipped_segment segment=%d line=%d speaker=%s clipped_samples=%llu path=%s\n",
                stats.segment_index,
                stats.source_line,
                speaker ? speaker : "",
                (unsigned long long)stats.clipped,
                path);
        ctx->clipped_segments++;
    }

    if (stats.peak > ctx->peak) ctx->peak = stats.peak;
    ctx->frames += stats.frames;
    ctx->samples += stats.samples;
    ctx->sum_squares += stats.sum_squares;
    ctx->clipped += stats.clipped;

    free(path);
    free(speaker);
    return 0;
}

int main(int argc, char **argv) {
    const char *manifest_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.json";
    QcCtx ctx = {0};
    int result = foc_json_each_array_object(manifest_path, "segments", qc_segment, &ctx);
    if (result < 0) return 2;
    printf("manifest=%s\nsegments=%d\nbad_segments=%d\nsilent_segments=%d\nlow_peak_segments=%d\nclipped_segments=%d\nclipped_samples=%llu\nsample_rate=%d\nchannels=%d\nspeech_duration_seconds=%.3f\npeak=%d\npeak_dbfs=%.2f\nrms_dbfs=%.2f\n",
           manifest_path,
           ctx.count,
           ctx.bad,
           ctx.silent,
           ctx.low_peak_segments,
           ctx.clipped_segments,
           (unsigned long long)ctx.clipped,
           ctx.expected_rate,
           ctx.expected_channels,
           ctx.expected_rate ? (double)ctx.frames / (double)ctx.expected_rate : 0.0,
           ctx.peak,
           dbfs_from_peak(ctx.peak),
           dbfs_from_rms(ctx.sum_squares, ctx.samples));
    return (ctx.bad || ctx.silent) ? 1 : 0;
}
