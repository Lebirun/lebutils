#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "cu.h"

#define UNIT_KB 0
#define UNIT_MB 1
#define UNIT_GB 2
#define UNIT_HUMAN 3

int cmd_free(int argc, char **argv) {
    int unit_mode;
    int show_help;
    unsigned int mem_total;
    unsigned int mem_free;
    unsigned int mem_used;
    int i;
    const char *p;
    int fd;
    char buf[512];
    int n;
    char *line;
    
    unit_mode = UNIT_KB;
    show_help = 0;
    mem_total = 0;
    mem_free = 0;
    
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
        printf("Usage: free [OPTION]\n");
        printf("Display memory usage statistics.\n\n");
        printf("  -h         show sizes in human readable format\n");
        printf("  -M         show sizes in MB\n");
        printf("  -G         show sizes in GB\n");
        printf("  --help     display this help\n");
        return 0;
    }
    
    fd = vfs_open("/proc/meminfo", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "free: cannot open /proc/meminfo\n");
        return 1;
    }
    
    n = vfs_read_fd(fd, buf, sizeof(buf) - 1);
    vfs_close_fd(fd);
    
    if (n < 0) {
        fprintf(stderr, "free: cannot read /proc/meminfo\n");
        return 1;
    }
    buf[n] = '\0';
    
    line = buf;
    while (*line) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            p = line + 9;
            while (*p == ' ' || *p == '\t') p++;
            mem_total = 0;
            while (*p >= '0' && *p <= '9') {
                mem_total = mem_total * 10 + (*p - '0');
                p++;
            }
        }
        else if (strncmp(line, "MemFree:", 8) == 0) {
            p = line + 8;
            while (*p == ' ' || *p == '\t') p++;
            mem_free = 0;
            while (*p >= '0' && *p <= '9') {
                mem_free = mem_free * 10 + (*p - '0');
                p++;
            }
        }
        
        while (*line && *line != '\n') line++;
        if (*line == '\n') line++;
    }
    
    mem_used = mem_total - mem_free;
    
    printf("              total        used        free\n");
    
    switch (unit_mode) {
    case UNIT_MB: {
        unsigned int t_i = mem_total / 1024;
        unsigned int t_d = (mem_total % 1024) * 10 / 1024;
        unsigned int u_i = mem_used / 1024;
        unsigned int u_d = (mem_used % 1024) * 10 / 1024;
        unsigned int f_i = mem_free / 1024;
        unsigned int f_d = (mem_free % 1024) * 10 / 1024;
        printf("Mem:       %5u.%u MB  %5u.%u MB  %5u.%u MB\n", 
               t_i, t_d, u_i, u_d, f_i, f_d);
        break;
    }
    case UNIT_GB: {
        unsigned int t_i = mem_total / 1048576;
        unsigned int t_d = (mem_total % 1048576) * 100 / 1048576;
        unsigned int u_i = mem_used / 1048576;
        unsigned int u_d = (mem_used % 1048576) * 100 / 1048576;
        unsigned int f_i = mem_free / 1048576;
        unsigned int f_d = (mem_free % 1048576) * 100 / 1048576;
        printf("Mem:       %5u.%02u GB  %5u.%02u GB  %5u.%02u GB\n", 
               t_i, t_d, u_i, u_d, f_i, f_d);
        break;
    }
    case UNIT_HUMAN: {
        if (mem_total >= 1048576) {
            unsigned int t_i = mem_total / 1048576;
            unsigned int t_d = (mem_total % 1048576) * 100 / 1048576;
            unsigned int u_i = mem_used / 1048576;
            unsigned int u_d = (mem_used % 1048576) * 100 / 1048576;
            unsigned int f_i = mem_free / 1048576;
            unsigned int f_d = (mem_free % 1048576) * 100 / 1048576;
            printf("Mem:       %5u.%02u GB  %5u.%02u GB  %5u.%02u GB\n", 
                   t_i, t_d, u_i, u_d, f_i, f_d);
        } else {
            unsigned int t_i = mem_total / 1024;
            unsigned int t_d = (mem_total % 1024) * 10 / 1024;
            unsigned int u_i = mem_used / 1024;
            unsigned int u_d = (mem_used % 1024) * 10 / 1024;
            unsigned int f_i = mem_free / 1024;
            unsigned int f_d = (mem_free % 1024) * 10 / 1024;
            printf("Mem:       %5u.%u MB  %5u.%u MB  %5u.%u MB\n", 
                   t_i, t_d, u_i, u_d, f_i, f_d);
        }
        break;
    }
    default:
        printf("Mem:       %8u KB  %8u KB  %8u KB\n", 
               mem_total, mem_used, mem_free);
        break;
    }
    
    return 0;
}
