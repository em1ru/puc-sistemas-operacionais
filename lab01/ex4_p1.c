#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int pid;
    
    pid = fork();

    if (pid != 0) { //pai 
        waitpid(pid, NULL, 0);
    }
    else { //filho 
        execv("./alomundo", NULL);
        exit(1);
    }
    return 0;
}