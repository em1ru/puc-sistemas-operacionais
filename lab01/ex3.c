#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

int* v = (int*)malloc(10*sizeof(int));

for (int i = 0; i < 10; i++) {
    v[i] = 9 - i;
}

int main() {
    int pid;

    for (int i = 0; i < 10; i++) {
        waitpid(pid, NULL, 0);
        printf("(pai antes) v[%d] = %d\n", i, v[i]);
    }
    
    pid = fork();

    if (pid != 0) { //pai 
        waitpid(pid, NULL, 0);
        for (int i = 0; i < 10; i++) {
            printf("(pai depois) v[%d] = %d\n", i, v[i]);
        }
    }
    else { //filho 
        for (int i = 0; i < 10; i++) {
            v[i] = i;
        }
        exit(1);
    }
    return 0;
}