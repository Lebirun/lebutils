#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "cu.h"

#define O_RDONLY 0

int cmd_free(int argc, char **argv) {
    int human_readable;
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
    unsigned int total_mb;
    unsigned int used_mb;
    unsigned int free_mb;
    
    human_readable = 0;
    show_help = 0;
    mem_total = 0;
    mem_free = 0;
    
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            show_help = 1;
        }
        if (argv[i][0] == '-') {
            for (p = argv[i] + 1; *p; p++) {
                if (*p == 'h') human_readable = 1;
                else if (*p == '?') show_help = 1;
            }
        }
    }
    
    if (show_help) {
        printf("Usage: free [OPTION]\n");
        printf("Display memory usage statistics.\n\n");
        printf("  -h         show sizes in human readable format\n");
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
    
    if (human_readable) {
        total_mb = mem_total / 1024;
        used_mb = mem_used / 1024;
        free_mb = mem_free / 1024;
        
        printf("              total        used        free\n");
        printf("Mem:       %8u MB  %8u MB  %8u MB\n", 
               total_mb, used_mb, free_mb);
    } else {
        printf("              total        used        free\n");
        printf("Mem:       %8u    %8u    %8u\n", 
               mem_total, mem_used, mem_free);
    }
    
    return 0;
}
