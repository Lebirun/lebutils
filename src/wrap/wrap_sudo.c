#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char **nargv;
    char *applet;
    int i;

    applet = (char *)"sudo";
    if (argc < 1) argc = 1;
    nargv = calloc((size_t)argc + 1, sizeof(*nargv));
    if (!nargv) {
        fprintf(stderr, "sudo: cannot allocate argument list\n");
        return 127;
    }
    nargv[0] = applet;
    for (i = 1; i < argc; i++) nargv[i] = argv[i];
    execv("/bin/lebu", nargv);
    free(nargv);
    return 127;
}
