#include <unistd.h>

int main(int argc, char **argv) {
    char *nargv[64];
    char *applet;
    int i;

    applet = (char *)"ps";
    if (argc < 1) argc = 1;
    if (argc > 63) argc = 63;
    nargv[0] = applet;
    for (i = 1; i < argc; i++) nargv[i] = argv[i];
    nargv[argc] = 0;
    execv("/bin/lebu", nargv);
    return 127;
}
