#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "cu.h"

#define UNIT_KB 0
#define UNIT_MB 1
#define UNIT_GB 2
#define UNIT_HUMAN 3

static void free_write_all(int fd, const char *data, size_t size) {
    ssize_t written;

    while (size > 0) {
        written = write(fd, data, size);
        if (written <= 0) return;
        data += written;
        size -= (size_t)written;
    }
}

static char *free_append(char *out, const char *text) {
    while (*text) *out++ = *text++;
    return out;
}

static char *free_append_uint(char *out, unsigned int value,
                              unsigned int width, int zero_pad) {
    char digits[16];
    unsigned int length;
    unsigned int padding;

    length = 0;
    do {
        digits[length++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    padding = width > length ? width - length : 0;
    while (padding--) *out++ = zero_pad ? '0' : ' ';
    while (length) *out++ = digits[--length];
    return out;
}

static char *free_append_scaled(char *out, unsigned int value,
                                unsigned int divisor, unsigned int width,
                                unsigned int decimals, const char *suffix) {
    unsigned int integer;
    unsigned int fraction;
    unsigned int scale;

    integer = value / divisor;
    scale = decimals == 1 ? 10 : 100;
    fraction = (value % divisor) * scale / divisor;
    out = free_append_uint(out, integer, width, 0);
    if (decimals) {
        *out++ = '.';
        out = free_append_uint(out, fraction, decimals, 1);
    }
    return free_append(out, suffix);
}

static unsigned int free_read_value(const char *line) {
    const char *p;
    unsigned int value;

    p = line;
    while (*p && *p != ':') p++;
    if (*p) p++;
    while (*p == ' ' || *p == '\t') p++;
    value = 0;
    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (unsigned int)(*p - '0');
        p++;
    }
    return value;
}

int cmd_free(int argc, char **argv) {
    int unit_mode;
    int show_help;
    unsigned int mem_total;
    unsigned int mem_free;
    unsigned int mem_used;
    unsigned int mem_all_used;
    int i;
    const char *p;
    int fd;
    char buf[512];
    int n;
    char *line;
    char out[256];
    char *out_end;
    unsigned int divisor;
    unsigned int width;
    unsigned int decimals;
    const char *prefix;
    const char *suffix;
    const char *last_suffix;
    
    unit_mode = UNIT_KB;
    show_help = 0;
    mem_total = 0;
    mem_free = 0;
    mem_used = 0;
    mem_all_used = 0;
    
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            show_help = 1;
        }
        if (argv[i][0] == '-') {
            for (p = argv[i] + 1; *p; p++) {
                if (*p == 'h') unit_mode = UNIT_HUMAN;
                else if (*p == 'M') unit_mode = UNIT_MB;
                else if (*p == 'G') unit_mode = UNIT_GB;
                else if (*p == '?') show_help = 1;
            }
        }
    }
    
    if (show_help) {
        free_write_all(1,
            "Usage: free [OPTION]\n"
            "Display memory usage statistics.\n\n"
            "  -h         show sizes in human readable format\n"
            "  -M         show sizes in MB\n"
            "  -G         show sizes in GB\n"
            "  --help     display this help\n",
            sizeof("Usage: free [OPTION]\n"
                   "Display memory usage statistics.\n\n"
                   "  -h         show sizes in human readable format\n"
                   "  -M         show sizes in MB\n"
                   "  -G         show sizes in GB\n"
                   "  --help     display this help\n") - 1);
        return 0;
    }
    
    fd = vfs_open("/proc/meminfo", O_RDONLY);
    if (fd < 0) {
        free_write_all(2, "free: cannot open /proc/meminfo\n",
                       sizeof("free: cannot open /proc/meminfo\n") - 1);
        return 1;
    }
    
    n = vfs_read_fd(fd, buf, sizeof(buf) - 1);
    vfs_close_fd(fd);
    
    if (n < 0) {
        free_write_all(2, "free: cannot read /proc/meminfo\n",
                       sizeof("free: cannot read /proc/meminfo\n") - 1);
        return 1;
    }
    buf[n] = '\0';
    
    line = buf;
    while (*line) {
        if (line[0] == 'M' && line[1] == 'e' && line[2] == 'm') {
            if (line[3] == 'T') mem_total = free_read_value(line);
            else if (line[3] == 'F') mem_free = free_read_value(line);
            else if (line[3] == 'U') mem_used = free_read_value(line);
            else if (line[3] == 'A' && line[4] == 'l')
                mem_all_used = free_read_value(line);
        }
        
        while (*line && *line != '\n') line++;
        if (*line == '\n') line++;
    }
    
    if (mem_used == 0 && mem_all_used == 0) {
        mem_all_used = mem_total - mem_free;
        mem_used = mem_all_used;
    }
    
    free_write_all(1, "                 total         used     all used         free\n",
                   sizeof("                 total         used     all used         free\n") - 1);

    divisor = 1;
    width = 8;
    decimals = 0;
    prefix = "Mem:       ";
    suffix = " KB  ";
    last_suffix = " KB";
    if (unit_mode == UNIT_MB ||
        (unit_mode == UNIT_HUMAN && mem_total < 1048576)) {
        divisor = 1024;
        width = 6;
        decimals = unit_mode == UNIT_HUMAN ? 1 : 2;
        prefix = unit_mode == UNIT_HUMAN ? "Mem:       " : "Mem:      ";
        suffix = unit_mode == UNIT_HUMAN ? " MB  " : " MB ";
        last_suffix = " MB";
    } else if (unit_mode == UNIT_GB || unit_mode == UNIT_HUMAN) {
        divisor = 1048576;
        width = 5;
        decimals = 2;
        suffix = " GB  ";
        last_suffix = " GB";
    }
    out_end = free_append(out, prefix);
    out_end = free_append_scaled(out_end, mem_total, divisor, width,
                                 decimals, suffix);
    out_end = free_append_scaled(out_end, mem_used, divisor, width,
                                 decimals, suffix);
    out_end = free_append_scaled(out_end, mem_all_used, divisor, width,
                                 decimals, suffix);
    out_end = free_append_scaled(out_end, mem_free, divisor, width,
                                 decimals, last_suffix);
    *out_end++ = '\n';
    free_write_all(1, out, (size_t)(out_end - out));
    
    return 0;
}
