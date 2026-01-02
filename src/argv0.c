#include <string.h>
#include "cu.h"

const char *cu_basename(const char *path) {
    if (!path) return "";
    const char *p = path;
    const char *last = path;
    while (*p) {
        if (*p == '/') last = p + 1;
        p++;
    }
    return last;
}
