#include "foc_common.h"
#include <stdio.h>

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.txt";
    FocScript script = {0};
    if (foc_load_script(path, &script, 1) != 0) return 2;
    printf("script=%s\nsegments=%zu\nstatus=ok\n", path, script.count);
    foc_free_script(&script);
    return 0;
}
