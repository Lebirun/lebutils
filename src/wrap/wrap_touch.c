#include <unistd.h>

int main(int argc, char **argv) {
    char *nargv[64];
    int i;

    if (argc > 60) argc = 60;
    nargv[0] = "lebcu";
    nargv[1] = "touch";
    for (i = 1; i < argc; i++) nargv[i + 1] = argv[i];
    nargv[argc + 1] = 0;
    execve("/bin/lebcu", nargv, 0);
    return 127;
}
