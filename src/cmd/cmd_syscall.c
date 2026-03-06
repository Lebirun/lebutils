#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "cu.h"

struct syscall_entry {
    const char *name;
    int num;
};

static const struct syscall_entry syscall_table[] = {
    {"exit", 0},
    {"write", 1},
    {"getpid", 2},
    {"read", 3},
    {"yield", 4},
    {"sleep", 5},
    {"waitpid", 6},
    {"sbrk", 7},
    {"mmap", 8},
    {"kill", 9},
    {"getticks", 10},
    {"time", 11},
    {"isatty", 12},
    {"fork", 13},
    {"exec", 14},
    {"initrd_count", 15},
    {"initrd_stat", 16},
    {"initrd_read", 17},
    {"open", 18},
    {"close", 19},
    {"fstat", 20},
    {"fb_putpixel", 21},
    {"fb_setcolors", 22},
    {"fb_getinfo", 23},
    {"fb_clear", 24},
    {"console_switch", 25},
    {"console_getcur", 26},
    {"console_clear", 27},
    {"vfs_open", 28},
    {"vfs_close", 29},
    {"vfs_read", 30},
    {"vfs_readdir", 31},
    {"vfs_stat", 32},
    {"vfs_mounts", 33},
    {"vfs_write", 34},
    {"vfs_create", 35},
    {"vfs_mkdir", 36},
    {"vfs_unlink", 37},
    {"console_setcursor", 38},
    {"read_nb", 39},
    {"sata_test", 40},
    {"sata_info", 41},
    {"sata_smart", 42},
    {"sata_irq", 43},
    {"net_ifconfig", 44},
    {"net_ping", 45},
    {"net_arp", 46},
    {"net_dns", 47},
    {"net_dhcp", 48},
    {"net_getinfo", 49},
    {"net_arp_get", 50},
    {"net_ping_one", 51},
    {"net_dns_resolve", 52},
    {"net_http_get", 53},
    {"tcgetattr", 54},
    {"tcsetattr", 55},
    {"ioctl", 56},
    {"tcflush", 57},
    {"tcflow", 58},
    {"tcdrain", 59},
    {"tcgetpgrp", 60},
    {"tcsetpgrp", 61},
    {"writev", 62},
    {"lseek", 63},
    {"execve", 64},
    {"dup", 65},
    {"dup2", 66},
    {"pipe", 67},
    {"sigaction", 68},
    {"sigprocmask", 69},
    {"stat", 71},
    {"getcwd", 72},
    {"chdir", 73},
    {"access", 74},
    {"clock_gettime", 75},
    {"gettimeofday", 76},
    {"munmap", 77},
    {"mprotect", 78},
    {"fcntl", 79},
    {"getdents", 80},
    {"truncate", 81},
    {"ftruncate", 82},
    {"rename", 83},
    {"link", 84},
    {"symlink", 85},
    {"readlink", 86},
    {"umask", 87},
    {"select", 88},
    {"poll", 89},
    {"ppoll", 90},
    {"socket", 91},
    {"socketpair", 92},
    {"bind", 93},
    {"connect", 94},
    {"listen", 95},
    {"accept", 96},
    {"accept4", 97},
    {"getsockopt", 98},
    {"setsockopt", 99},
    {"getsockname", 100},
    {"getpeername", 101},
    {"sendto", 102},
    {"sendmsg", 103},
    {"recvfrom", 104},
    {"recvmsg", 105},
    {"shutdown", 106},
    {"openat", 107},
    {"mkdirat", 108},
    {"mknodat", 109},
    {"fchownat", 110},
    {"unlinkat", 111},
    {"renameat", 112},
    {"linkat", 113},
    {"symlinkat", 114},
    {"readlinkat", 115},
    {"fchmodat", 116},
    {"faccessat", 117},
    {"fstatat", 118},
    {"utimensat", 119},
    {"renameat2", 120},
    {"rt_sigaction", 121},
    {"rt_sigprocmask", 122},
    {"rt_sigpending", 123},
    {"rt_sigsuspend", 124},
    {"rt_sigreturn", 125},
    {"rt_sigtimedwait", 126},
    {"rt_sigqueueinfo", 127},
    {"tgkill", 128},
    {"tkill", 129},
    {"sigaltstack", 130},
    {"pause", 131},
    {"alarm", 132},
    {"getuid", 133},
    {"getgid", 134},
    {"geteuid", 135},
    {"getegid", 136},
    {"setuid", 137},
    {"setgid", 138},
    {"seteuid", 139},
    {"setegid", 140},
    {"setreuid", 141},
    {"setregid", 142},
    {"setresuid", 143},
    {"setresgid", 144},
    {"getresuid", 145},
    {"getresgid", 146},
    {"setfsuid", 147},
    {"setfsgid", 148},
    {"getgroups", 149},
    {"setgroups", 150},
    {"getpgid", 151},
    {"setpgid", 152},
    {"getpgrp", 153},
    {"setsid", 154},
    {"getsid", 155},
    {"getppid", 156},
    {"getpid2", 157},
    {"gettid", 158},
    {"uname", 159},
    {"sysinfo", 160},
    {"getrlimit", 161},
    {"setrlimit", 162},
    {"getrusage", 163},
    {"prlimit64", 164},
    {"mmap2", 165},
    {"mremap", 166},
    {"madvise", 167},
    {"mincore", 168},
    {"pread64", 169},
    {"pwrite64", 170},
    {"readv", 171},
    {"fchdir", 172},
    {"fchmod", 173},
    {"fchown", 174},
    {"fsync", 175},
    {"fdatasync", 176},
    {"flock", 177},
    {"getdents64", 178},
    {"dup3", 179},
    {"pipe2", 180},
    {"eventfd", 181},
    {"eventfd2", 182},
    {"epoll_create", 183},
    {"epoll_create1", 184},
    {"epoll_ctl", 185},
    {"epoll_wait", 186},
    {"epoll_pwait", 187},
    {"set_tid_address", 188},
    {"futex", 189},
    {"set_robust_list", 190},
    {"get_robust_list", 191},
    {"clone", 192},
    {"vfork", 193},
    {"wait4", 194},
    {"waitid", 195},
    {"getrandom", 196},
    {"prctl", 197},
    {"arch_prctl", 198},
    {"timerfd_create", 199},
    {"timerfd_settime", 200},
    {"timerfd_gettime", 201},
    {"signalfd", 202},
    {"signalfd4", 203},
    {"inotify_init", 204},
    {"inotify_init1", 205},
    {"inotify_add_watch", 206},
    {"inotify_rm_watch", 207},
    {"posix_openpt", 208},
    {"grantpt", 209},
    {"unlockpt", 210},
    {"ptsname", 211},
    {"setitimer", 212},
    {"getitimer", 213},
    {"chmod", 214},
    {"chown", 215},
    {"lchown", 216},
    {"nanosleep", 217},
    {"sigreturn", 218},
    {"setenv", 219},
    {"getenv", 220},
    {"unsetenv", 221},
    {"clearenv", 222},
    {"pthread_create", 223},
    {"pthread_exit", 224},
    {"pthread_join", 225},
    {"pthread_detach", 226},
    {"pthread_self", 227},
    {"pthread_mutex_init", 228},
    {"pthread_mutex_destroy", 229},
    {"pthread_mutex_lock", 230},
    {"pthread_mutex_trylock", 231},
    {"pthread_mutex_unlock", 232},
    {"pthread_cond_init", 233},
    {"pthread_cond_destroy", 234},
    {"pthread_cond_wait", 235},
    {"pthread_cond_signal", 236},
    {"pthread_cond_broadcast", 237},
    {"shmget", 238},
    {"shmat", 239},
    {"shmdt", 240},
    {"shmctl", 241},
    {"shm_open", 242},
    {"shm_unlink", 243},
    {"dlopen", 244},
    {"dlsym", 245},
    {"dlclose", 246},
    {"dlerror", 247},
    {"regcomp", 248},
    {"regexec", 249},
    {"regfree", 250},
    {"regerror", 251},
    {"fnmatch", 252},
    {"glob", 253},
    {"globfree", 254},
    {"sscanf", 255},
    {"scanf_getchar", 256},
    {"regsub", 257},
    {"regexec_ex", 258},
    {"fb_set_mode", 259},
    {"fb_get_detailed_info", 260},
    {"fb_get_caps", 261},
    {"statfs", 262},
    {"fstatfs", 263},
    {"net_http_post", 264},
    {"pselect6", 265},
    {"getpriority", 266},
    {"setpriority", 267},
    {"pivot_root", 268},
    {"reboot", 269},
    {"console_setid", 270},
    {"crypto", 271},
    {"vfs_mount", 272},
    {"vfs_umount", 273},
    {NULL, 0}
};

#define SYSCALL_TABLE_COUNT (sizeof(syscall_table) / sizeof(syscall_table[0]) - 1)

static inline int do_syscall0(int num)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num)
        : "memory"
    );
    return ret;
}

static inline int do_syscall1(int num, int arg1)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1)
        : "memory"
    );
    return ret;
}

static inline int do_syscall2(int num, int arg1, int arg2)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2)
        : "memory"
    );
    return ret;
}

static inline int do_syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory"
    );
    return ret;
}

static int resolve_syscall(const char *s)
{
    char *end;
    long val;
    size_t i;
    char lower[64];
    size_t len;

    val = strtol(s, &end, 0);
    if (end != s && *end == '\0')
        return (int)val;

    len = strlen(s);
    if (len >= sizeof(lower))
        len = sizeof(lower) - 1;
    for (i = 0; i < len; i++)
        lower[i] = tolower((unsigned char)s[i]);
    lower[len] = '\0';

    for (i = 0; i < SYSCALL_TABLE_COUNT; i++) {
        if (strcmp(lower, syscall_table[i].name) == 0)
            return syscall_table[i].num;
    }

    return -1;
}

static const char *syscall_name(int num)
{
    size_t i;

    for (i = 0; i < SYSCALL_TABLE_COUNT; i++) {
        if (syscall_table[i].num == num)
            return syscall_table[i].name;
    }
    return NULL;
}

static int parse_arg(const char *s)
{
    return (int)strtol(s, NULL, 0);
}

static void hexdump(const unsigned char *buf, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        printf("%02x", buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
        else if ((i + 1) % 4 == 0)
            printf("  ");
        else
            printf(" ");
    }
    if (len > 0 && len % 16 != 0)
        printf("\n");
}

static void print_list(const char *filter)
{
    size_t i;
    char lower[64];
    size_t len;
    size_t j;

    if (filter) {
        len = strlen(filter);
        if (len >= sizeof(lower))
            len = sizeof(lower) - 1;
        for (j = 0; j < len; j++)
            lower[j] = tolower((unsigned char)filter[j]);
        lower[len] = '\0';
    }

    printf("%-4s  %s\n", "NR", "NAME");
    for (i = 0; i < SYSCALL_TABLE_COUNT; i++) {
        if (filter && !strstr(syscall_table[i].name, lower))
            continue;
        printf("%-4d  %s\n", syscall_table[i].num, syscall_table[i].name);
    }
}

static void print_usage(void)
{
    printf("Usage: syscall [options] <nr|name> [arg1] [arg2] [arg3]\n");
    printf("\n");
    printf("Invoke a raw kernel syscall by number or name.\n");
    printf("Arguments can be decimal, hex (0x...), or octal (0...).\n");
    printf("\n");
    printf("Options:\n");
    printf("  --buf <size>     Allocate buffer as arg1, size as arg2.\n");
    printf("                   Hexdumps result. Extra args shift to arg3.\n");
    printf("  --str <string>   Pass string pointer as next arg, strlen as\n");
    printf("                   the arg after. Can be used multiple times.\n");
    printf("  --list [filter]  List all known syscalls (optional filter).\n");
    printf("\n");
    printf("Examples:\n");
    printf("  syscall getpid\n");
    printf("  syscall write 1 --str \"hello\\n\"\n");
    printf("  syscall --buf 256 getcwd\n");
    printf("  syscall --list net\n");
    printf("  syscall 2\n");
}

int cmd_syscall(int argc, char **argv)
{
    int nr;
    int args[3];
    int nargs;
    int ret;
    int buf_mode;
    int buf_size;
    unsigned char *buf;
    int argi;
    const char *name;
    const char *nr_str;

    buf_mode = 0;
    buf_size = 0;
    buf = NULL;
    argi = 1;
    nargs = 0;
    args[0] = 0;
    args[1] = 0;
    args[2] = 0;
    nr = -1;

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0) {
        print_usage();
        return (argc < 2) ? 1 : 0;
    }

    if (strcmp(argv[1], "--list") == 0) {
        print_list((argc > 2) ? argv[2] : NULL);
        return 0;
    }

    while (argi < argc) {
        if (strcmp(argv[argi], "--buf") == 0) {
            if (argi + 1 >= argc) {
                printf("syscall: --buf requires <size>\n");
                return 1;
            }
            buf_mode = 1;
            argi++;
            buf_size = (int)strtol(argv[argi], NULL, 0);
            if (buf_size <= 0 || buf_size > 4096) {
                printf("syscall: buffer size must be 1-4096\n");
                return 1;
            }
            argi++;
        } else if (strcmp(argv[argi], "--str") == 0) {
            if (argi + 1 >= argc) {
                printf("syscall: --str requires a string argument\n");
                return 1;
            }
            argi++;
            if (nargs < 3)
                args[nargs++] = (int)(unsigned int)argv[argi];
            if (nargs < 3)
                args[nargs++] = (int)strlen(argv[argi]);
            argi++;
        } else if (nr == -1) {
            nr_str = argv[argi];
            nr = resolve_syscall(nr_str);
            if (nr < 0) {
                printf("syscall: unknown syscall '%s'\n", nr_str);
                printf("Use 'syscall --list' to see available syscalls.\n");
                return 1;
            }
            argi++;
        } else {
            if (nargs < 3)
                args[nargs++] = parse_arg(argv[argi]);
            argi++;
        }
    }

    if (nr < 0) {
        printf("syscall: missing syscall number or name\n");
        return 1;
    }

    name = syscall_name(nr);

    if (buf_mode) {
        buf = (unsigned char *)malloc(buf_size);
        if (!buf) {
            printf("syscall: failed to allocate buffer\n");
            return 1;
        }
        memset(buf, 0, buf_size);
        ret = do_syscall3(nr, (int)(unsigned int)buf, buf_size,
                          (nargs > 0) ? args[0] : 0);
        if (name)
            printf("syscall(%s[%d], buf, %d) = %d (0x%08x)\n",
                   name, nr, buf_size, ret, (unsigned int)ret);
        else
            printf("syscall(%d, buf, %d) = %d (0x%08x)\n",
                   nr, buf_size, ret, (unsigned int)ret);
        hexdump(buf, (ret > 0 && ret <= buf_size) ? ret : buf_size);
        free(buf);
    } else {
        if (nargs == 0)
            ret = do_syscall0(nr);
        else if (nargs == 1)
            ret = do_syscall1(nr, args[0]);
        else if (nargs == 2)
            ret = do_syscall2(nr, args[0], args[1]);
        else
            ret = do_syscall3(nr, args[0], args[1], args[2]);

        if (name)
            printf("syscall(%s[%d]) = %d (0x%08x)\n",
                   name, nr, ret, (unsigned int)ret);
        else
            printf("syscall(%d) = %d (0x%08x)\n",
                   nr, ret, (unsigned int)ret);
    }

    return 0;
}
