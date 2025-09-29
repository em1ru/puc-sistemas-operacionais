#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define BUFFER_SIZE 256

int main(int argc, char* argv[]) {
    int fd[2];

    if (pipe(fd) < 0) {
        perror("failed to open pipe");
        exit(-1);
    }

    int pid_1, pid_2;

    pid_1 = fork();

    if (pid_1 < 0) {
        perror("failed to fork");
        exit(1);
    }

    if (pid_1 == 0) {
        // filho 1 roda o comando ps (process status)
        close(fd[0]); // apenas escreve
        dup2(fd[1], 1);
        execlp("ps", "ps", NULL);
    }
    
    pid_2 = fork();

    if (pid_2 == 0) {
        // filho 2 roda o comando wc (word count), recebe o stdout da execução do filho 1
        close(fd[1]); // apenas lê
        waitpid(pid_1, NULL, 0);
        dup2(fd[0], 0);
        
        execlp("wc", "wc", NULL);
    }

    close(fd[0]);
    close(fd[1]);
    
    waitpid(-1, NULL, 0);

    return 0;
}