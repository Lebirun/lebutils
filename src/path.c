#include <string.h>
#include <unistd.h>
#include "cu.h"

static int cu_is_sep(char c) { return c == '/'; }

int cu_path_abs(const char *in, char *out, unsigned int outsz) {
    if (!out || outsz == 0) return -1;
    out[0] = '\0';

    if (!in || !*in) return -1;

    char tmp[512];
    tmp[0] = '\0';

    if (in[0] == '/') {
        unsigned int n = (unsigned int)strlen(in);
        if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
        memcpy(tmp, in, n);
        tmp[n] = '\0';
    } else {
        char cwd[256];
        if (!getcwd(cwd, sizeof(cwd))) return -1;
        if (strcmp(cwd, "/") == 0) {
            unsigned int need = 1 + (unsigned int)strlen(in);
            if (need >= sizeof(tmp)) need = sizeof(tmp) - 1;
            tmp[0] = '/';
            tmp[1] = '\0';
            strncat(tmp, in, sizeof(tmp) - 2);
        } else {
            unsigned int clen = (unsigned int)strlen(cwd);
            unsigned int ilen = (unsigned int)strlen(in);
            unsigned int pos = 0;
            if (clen >= sizeof(tmp)) clen = sizeof(tmp) - 1;
            memcpy(tmp, cwd, clen);
            pos = clen;
            if (pos + 1 < sizeof(tmp)) tmp[pos++] = '/';
            unsigned int can = sizeof(tmp) - 1 - pos;
            if (ilen > can) ilen = can;
            memcpy(tmp + pos, in, ilen);
            pos += ilen;
            tmp[pos] = '\0';
        }
    }

    const char *parts[32];
    int pc = 0;
    char buf[512];
    unsigned int tlen = (unsigned int)strlen(tmp);
    if (tlen >= sizeof(buf)) tlen = sizeof(buf) - 1;
    memcpy(buf, tmp, tlen);
    buf[tlen] = '\0';

    char *p = buf;
    while (*p) {
        while (cu_is_sep(*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && !cu_is_sep(*p)) p++;
        if (*p) *p++ = '\0';
        if (strcmp(start, ".") == 0) continue;
        if (strcmp(start, "..") == 0) {
            if (pc > 0) pc--;
            continue;
        }
        if (pc < (int)(sizeof(parts) / sizeof(parts[0]))) parts[pc++] = start;
    }

    if (pc == 0) {
        if (outsz < 2) return -1;
        out[0] = '/';
        out[1] = '\0';
        return 0;
    }

    unsigned int pos = 0;
    out[pos++] = '/';
    for (int i = 0; i < pc; i++) {
        unsigned int len = (unsigned int)strlen(parts[i]);
        if (pos + len + 1 >= outsz) break;
        memcpy(out + pos, parts[i], len);
        pos += len;
        if (i + 1 < pc) out[pos++] = '/';
    }
    out[pos] = '\0';
    return 0;
}
