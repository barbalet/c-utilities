#include "foc_common.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int count;
    int missing_cache;
    int bad_cache;
    int monotonic_ok;
    double last_end;
    double speech_seconds;
    double segment_seconds;
    int sample_rate;
} Summary;

static int summarize_segment(const char *object, size_t len, void *raw) {
    Summary *summary = (Summary *)raw;
    double start = foc_json_get_number_slice(object, len, "start_seconds", -1.0);
    double speech_end = foc_json_get_number_slice(object, len, "speech_end_seconds", -1.0);
    double end = foc_json_get_number_slice(object, len, "end_seconds", speech_end);
    long sample_rate = foc_json_get_long_slice(object, len, "sample_rate_hz", 0);
    char *cache_path = foc_json_get_string_slice(object, len, "segment_cache_audio");
    FocWavInfo info;

    summary->count++;
    if (sample_rate && !summary->sample_rate) summary->sample_rate = (int)sample_rate;
    if (start + 0.0005 < summary->last_end) summary->monotonic_ok = 0;
    if (speech_end >= start) summary->speech_seconds += speech_end - start;
    if (end >= start) summary->segment_seconds += end - start;
    if (end > summary->last_end) summary->last_end = end;

    if (cache_path) {
        if (foc_wav_info(cache_path, &info) != 0) {
            summary->bad_cache++;
        } else if (!summary->sample_rate) {
            summary->sample_rate = info.sample_rate;
        }
        free(cache_path);
    } else {
        summary->missing_cache++;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *manifest_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.json";
    size_t size = 0;
    char *data = foc_read_file(manifest_path, &size);
    Summary summary = {0};
    double declared_duration;
    long declared_count;
    long declared_rate;
    int result;

    if (!data) {
        perror(manifest_path);
        return 2;
    }

    summary.monotonic_ok = 1;
    declared_duration = foc_json_get_number_slice(data, size, "duration_seconds", -1.0);
    declared_count = foc_json_get_long_slice(data, size, "segment_count", -1);
    declared_rate = foc_json_get_long_slice(data, size, "sample_rate_hz", 0);
    free(data);

    result = foc_json_each_array_object(manifest_path, "segments", summarize_segment, &summary);
    if (result < 0) return 2;
    if (!summary.sample_rate && declared_rate > 0) summary.sample_rate = (int)declared_rate;

    printf("manifest=%s\nsegment_count_declared=%ld\nsegment_count_found=%d\nsample_rate_declared=%ld\nsample_rate_found=%d\nduration_declared_seconds=%.3f\nlast_end_seconds=%.3f\nspeech_seconds=%.3f\nsegment_seconds_including_silence=%.3f\nmonotonic=%s\nmissing_cache_paths=%d\nbad_cache_paths=%d\n",
           manifest_path,
           declared_count,
           summary.count,
           declared_rate,
           summary.sample_rate,
           declared_duration,
           summary.last_end,
           summary.speech_seconds,
           summary.segment_seconds,
           summary.monotonic_ok ? "yes" : "no",
           summary.missing_cache,
           summary.bad_cache);

    return (summary.bad_cache || !summary.monotonic_ok || (declared_count >= 0 && declared_count != summary.count)) ? 1 : 0;
}
