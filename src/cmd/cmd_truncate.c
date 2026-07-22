#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include "cu.h"

static int truncate_parse_size(const char *text, off_t current, off_t *result)
{
    unsigned long long value;
    unsigned long long multiplier;
    const char *p;
    char operation;

    if (!text || !*text || !result) return -1;

    operation = '=';
    p = text;
    if (*p == '+' || *p == '-') operation = *p++;
    if (*p < '0' || *p > '9') return -1;

    value = 0;
    while (*p >= '0' && *p <= '9') {
        if (value > (ULLONG_MAX - (unsigned long long)(*p - '0')) / 10ULL)
            return -1;
        value = value * 10ULL + (unsigned long long)(*p - '0');
        p++;
    }

    multiplier = 1;
    if (*p) {
        if (p[1] != '\0') return -1;
        if (*p == 'K' || *p == 'k') multiplier = 1024ULL;
        else if (*p == 'M' || *p == 'm') multiplier = 1024ULL * 1024ULL;
        else if (*p == 'G' || *p == 'g') multiplier = 1024ULL * 1024ULL * 1024ULL;
        else return -1;
    }

    if (value > ULLONG_MAX / multiplier) return -1;
    value *= multiplier;
    if (value > (unsigned long long)LLONG_MAX) return -1;

    if (operation == '+') {
        if (current < 0 || value > (unsigned long long)(LLONG_MAX - current)) return -1;
        *result = current + (off_t)value;
    } else if (operation == '-') {
        if (current < 0 || value > (unsigned long long)current) *result = 0;
        else *result = current - (off_t)value;
    } else {
        *result = (off_t)value;
    }
    return 0;
}

int cmd_truncate(int argc, char **argv)
{
    const char *size_text;
    const char *path;
    struct stat st;
    off_t size;
    off_t current;
    int no_create;
    int file_count;
    int i;
    int fd;
    int rc;

    size_text = NULL;
    no_create = 0;
    file_count = 0;
    rc = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: truncate [-c] -s SIZE FILE...");
            puts("Change the size of each FILE.");
            puts("  -c, --no-create  do not create missing files");
            puts("  -s, --size SIZE  set or adjust the file size");
            puts("SIZE may use K, M, or G and an optional + or - prefix.");
            return 0;
        }
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--no-create") == 0) {
            no_create = 1;
        } else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--size") == 0) && i + 1 < argc) {
            size_text = argv[++i];
        } else if (strncmp(argv[i], "--size=", 7) == 0) {
            size_text = argv[i] + 7;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "truncate: unknown option '%s'\n", argv[i]);
            return 1;
        } else {
            file_count++;
        }
    }

    if (!size_text || file_count == 0) {
        fprintf(stderr, "truncate: missing size or file operand\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        path = argv[i];
        if (strcmp(path, "-c") == 0 || strcmp(path, "--no-create") == 0) continue;
        if (strcmp(path, "-s") == 0 || strcmp(path, "--size") == 0) {
            i++;
            continue;
        }
        if (strncmp(path, "--size=", 7) == 0) continue;
        if (path[0] == '-') continue;

        current = 0;
        if (stat(path, &st) < 0) {
            if (no_create) continue;
            fd = open(path, O_WRONLY | O_CREAT, 0666);
            if (fd < 0) {
                fprintf(stderr, "truncate: cannot create '%s'\n", path);
                rc = 1;
                continue;
            }
            close(fd);
        } else {
            current = st.st_size;
        }

        if (truncate_parse_size(size_text, current, &size) < 0) {
            fprintf(stderr, "truncate: invalid size '%s'\n", size_text);
            return 1;
        }
        if (truncate(path, size) < 0) {
            fprintf(stderr, "truncate: cannot resize '%s'\n", path);
            rc = 1;
        }
    }
    return rc;
}
