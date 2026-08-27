#include <string.h>
#include "cu.h"

const char *cu_basename(const char *path) {
    const char *p;
    const char *last;

    if (!path) return "";
    p = path;
    last = path;
    while (*p) {
        if (*p == '/') last = p + 1;
        p++;
    }
    return last;
}
