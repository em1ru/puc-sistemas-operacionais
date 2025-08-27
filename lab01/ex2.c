#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    int pid, var;
    var = 1;
    printf("(pai antes) var = %d\n", var);

    pid = fork();

    if (pid != 0) { //pai 
        waitpid(pid, NULL, 0);
        printf("(pai depois) var = %d\n", var);

    }
    else { //filho 
        var = 5;
        printf("(filho) var = %d\n", var);
        exit(1);
    }
    return 0;
}