#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include "cu.h"

int cmd_uname(int argc, char **argv) {
    int show_all;
    int show_sysname;
    int show_nodename;
    int show_release;
    int show_version;
    int show_machine;
    int i;
    const char *p;
    int any_shown;
    struct utsname name;
    int ret;
    
    show_all = 0;
    show_sysname = 0;
    show_nodename = 0;
    show_release = 0;
    show_version = 0;
    show_machine = 0;
    any_shown = 0;
    
    ret = uname(&name);
    if (ret < 0) {
        fprintf(stderr, "uname: cannot get system information\n");
        return 1;
    }
    
    if (argc == 1) {
        show_sysname = 1;
    } else {
        for (i = 1; i < argc; i++) {
            if (argv[i][0] == '-') {
                for (p = argv[i] + 1; *p; p++) {
                    if (*p == 'a') {
                        show_all = 1;
                    } else if (*p == 's') {
                        show_sysname = 1;
                    } else if (*p == 'n') {
                        show_nodename = 1;
                    } else if (*p == 'r') {
                        show_release = 1;
                    } else if (*p == 'v') {
                        show_version = 1;
                    } else if (*p == 'm') {
                        show_machine = 1;
                    } else if (*p == '?' || *p == 'h') {
                        puts("Usage: uname [-asnrvm]\n");
                        puts("Print system information\n");
                        puts("  -a  Print all information");
                        puts("  -s  Print system name");
                        puts("  -n  Print network node hostname");
                        puts("  -r  Print operating system release");
                        puts("  -v  Print operating system version");
                        puts("  -m  Print machine hardware name");
                        puts("  --help  Display this help and exit");
                        return 0;
                    }
                }
            } else if (strcmp(argv[i], "--help") == 0) {
                puts("Usage: uname [-asnrvm]\n");
                puts("Print system information\n");
                puts("  -a  Print all information");
                puts("  -s  Print system name");
                puts("  -n  Print network node hostname");
                puts("  -r  Print operating system release");
                puts("  -v  Print operating system version");
                puts("  -m  Print machine hardware name");
                puts("  --help  Display this help and exit");
                return 0;
            }
        }
    }
    
    if (show_all) {
        show_sysname = 1;
        show_nodename = 1;
        show_release = 1;
        show_version = 1;
        show_machine = 1;
    }
    
    if (show_sysname) {
        if (any_shown) putchar(' ');
        printf("%s", name.sysname);
        any_shown = 1;
    }
    
    if (show_nodename) {
        if (any_shown) putchar(' ');
        printf("%s", name.nodename);
        any_shown = 1;
    }
    
    if (show_release) {
        if (any_shown) putchar(' ');
        printf("%s", name.release);
        any_shown = 1;
    }
    
    if (show_version) {
        if (any_shown) putchar(' ');
        printf("%s", name.version);
        any_shown = 1;
    }
    
    if (show_machine) {
        if (any_shown) putchar(' ');
        printf("%s", name.machine);
        any_shown = 1;
    }
    
    putchar('\n');
    return 0;
}
