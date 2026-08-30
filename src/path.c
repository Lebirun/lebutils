#include <string.h>
#include <unistd.h>
#include "cu.h"

static int cu_is_sep(char c)
{
    return c == '/';
}

static int __attribute__((cold))
cu_path_abs_slow(const char *in, char *out, unsigned int outsz)
{
    char tmp[512];
    char cwd[256];
    const char *parts[32];
    char buf[512];
    char *p;
    char *start;
    unsigned int n;
    unsigned int need;
    unsigned int clen;
    unsigned int ilen;
    unsigned int pos;
    unsigned int can;
    unsigned int tlen;
    unsigned int len;
    int pc;
    int i;

    tmp[0] = '\0';
    if (in[0] == '/') {
        n = (unsigned int)strlen(in);
        if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
        memcpy(tmp, in, n);
        tmp[n] = '\0';
    } else {
        if (!getcwd(cwd, sizeof(cwd))) return -1;
        if (strcmp(cwd, "/") == 0) {
            need = 1 + (unsigned int)strlen(in);
            if (need >= sizeof(tmp)) need = sizeof(tmp) - 1;
            tmp[0] = '/';
            tmp[1] = '\0';
            strncat(tmp, in, sizeof(tmp) - 2);
        } else {
            clen = (unsigned int)strlen(cwd);
            ilen = (unsigned int)strlen(in);
            pos = 0;
            if (clen >= sizeof(tmp)) clen = sizeof(tmp) - 1;
            memcpy(tmp, cwd, clen);
            pos = clen;
            if (pos + 1 < sizeof(tmp)) tmp[pos++] = '/';
            can = sizeof(tmp) - 1 - pos;
            if (ilen > can) ilen = can;
            memcpy(tmp + pos, in, ilen);
            pos += ilen;
            tmp[pos] = '\0';
        }
    }

    pc = 0;
    tlen = (unsigned int)strlen(tmp);
    if (tlen >= sizeof(buf)) tlen = sizeof(buf) - 1;
    memcpy(buf, tmp, tlen);
    buf[tlen] = '\0';
    p = buf;
    while (*p) {
        while (cu_is_sep(*p)) p++;
        if (!*p) break;
        start = p;
        while (*p && !cu_is_sep(*p)) p++;
        if (*p) *p++ = '\0';
        if (strcmp(start, ".") == 0) continue;
        if (strcmp(start, "..") == 0) {
            if (pc > 0) pc--;
            continue;
        }
        if (pc < (int)(sizeof(parts) / sizeof(parts[0])))
            parts[pc++] = start;
    }

    if (pc == 0) {
        if (outsz < 2) return -1;
        out[0] = '/';
        out[1] = '\0';
        return 0;
    }

    pos = 0;
    out[pos++] = '/';
    for (i = 0; i < pc; i++) {
        len = (unsigned int)strlen(parts[i]);
        if (pos + len + 1 >= outsz) break;
        memcpy(out + pos, parts[i], len);
        pos += len;
        if (i + 1 < pc) out[pos++] = '/';
    }
    out[pos] = '\0';
    return 0;
}

int cu_path_abs(const char *in, char *out, unsigned int outsz)
{
    const char *p;
    const char *segment;
    size_t length;
    size_t segment_length;
    size_t i;
    int normalized;

    if (!out || outsz == 0) return -1;
    out[0] = '\0';
    if (!in || !*in) return -1;
    if (in[0] != '/') return cu_path_abs_slow(in, out, outsz);

    normalized = 1;
    segment = in + 1;
    p = segment;
    while (*p) {
        if (*p++ != '/') continue;
        segment_length = (size_t)((p - 1) - segment);
        if (segment_length == 0 ||
            (segment_length == 1 && segment[0] == '.') ||
            (segment_length == 2 && segment[0] == '.' &&
             segment[1] == '.')) {
            normalized = 0;
            break;
        }
        segment = p;
    }
    segment_length = (size_t)(p - segment);
    if ((segment_length == 1 && segment[0] == '.') ||
        (segment_length == 2 && segment[0] == '.' && segment[1] == '.'))
        normalized = 0;
    if (p > in + 1 && p[-1] == '/') normalized = 0;
    if (!normalized) return cu_path_abs_slow(in, out, outsz);

    length = (size_t)(p - in);
    if (length >= outsz) return cu_path_abs_slow(in, out, outsz);
    for (i = 0; i <= length; i++) out[i] = in[i];
    return 0;
}
