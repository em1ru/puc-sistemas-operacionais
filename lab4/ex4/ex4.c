#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

#define MESSAGE_SIZE 2
#define DELTA_T 1

int main(int argc, char* argv[]) {
    int fd[2];
    
    if (pipe(fd) < 0) {
        perror("failed to pipe");
    }

    int w_sleep = DELTA_T, r_sleep = 2*w_sleep;
    int pid_leitor1, pid_leitor2;
    char buff1[MESSAGE_SIZE], buff2[MESSAGE_SIZE]; 

    const char message1[] = "A";
    const char message2[] = "B";

    int contador = 5;

    pid_leitor1 = fork();

    if (pid_leitor1 < 0) {
        perror("failed to fork 1");
        exit(1);
    }

    if (pid_leitor1 == 0) {
        // filho leitor 1
        close(fd[1]); // não lê apenas escreve
        while (contador) {
            sleep(r_sleep);
            read(fd[0], buff1, MESSAGE_SIZE);
            fprintf(stdout, "[leitor 1] filho 1 leu: %s\n", buff1);
            contador--;    
        }
        exit(3);
    }

    pid_leitor2 = fork();

    if (pid_leitor2 < 0) {
        perror("failed to fork 2");
        exit(2);
    }

    if (pid_leitor2 == 0) {
        // filho leitor 2
        close(fd[1]); // não lê apenas escreve
        while (contador) {
            sleep(r_sleep);
            read(fd[0], buff2, MESSAGE_SIZE);
            fprintf(stdout, "[leitor 2] filho 2 leu: %s\n", buff2);        
            contador--;    
        }
        exit(3);
    }

    close(fd[0]); // apenas escreve

    while (contador) {
        sleep(w_sleep);
        write(fd[1], message1, MESSAGE_SIZE);
        fprintf(stdout, "[escritor] pai escreveu\n");        
        write(fd[1], message2, MESSAGE_SIZE);         
        fprintf(stdout, "[escritor] pai escreveu\n");        
        contador--;    
    }
    sleep(r_sleep); // garantir última execução dos filhos;

    close(fd[1]);
    waitpid(-1, NULL, 0);

    return 0;
}