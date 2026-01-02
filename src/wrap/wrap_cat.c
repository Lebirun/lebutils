#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv) {
    char **nargv = malloc(sizeof(char *) * (unsigned int)(argc + 2));
    if (!nargv) return 1;
    nargv[0] = (char *)"lebcu";
    nargv[1] = (char *)"cat";
    for (int i = 1; i < argc; i++) nargv[i + 1] = argv[i];
    nargv[argc + 1] = 0;
    execve("/bin/lebcu", nargv, 0);
    printf("cat: exec failed\n");
    return 127;
}
