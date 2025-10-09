// Exercicio 2 - Dois filhos escrevem na FIFO, pai le tudo
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

#define FIFO "fifoEx2"

int main() {
    int fd;
    pid_t pid1, pid2;
    char buffer[256];
    int n;
    
    // cria a FIFO
    if (access(FIFO, F_OK) == -1) {
        if (mkfifo(FIFO, S_IRUSR | S_IWUSR) == -1) {
            perror("mkfifo");
            exit(1);
        }
    }
    
    // cria primeiro filho
    pid1 = fork();
    if (pid1 == 0) {
        // primeiro filho escreve mensagem
        sleep(1); // espera um pouco
        fd = open(FIFO, O_WRONLY);
        char *msg1 = "Mensagem do filho 1\n";
        write(fd, msg1, strlen(msg1));
        close(fd);
        exit(0);
    }
    
    // cria segundo filho
    pid2 = fork();
    if (pid2 == 0) {
        // segundo filho escreve mensagem
        sleep(1); // espera um pouco
        fd = open(FIFO, O_WRONLY);
        char *msg2 = "Mensagem do filho 2\n";
        write(fd, msg2, strlen(msg2));
        close(fd);
        exit(0);
    }
    
    // pai espera os filhos terminarem
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    
    printf("Filhos terminaram, lendo mensagens da FIFO:\n");
    
    // pai abre e le da FIFO
    fd = open(FIFO, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }
    
    // le e imprime tudo
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        write(1, buffer, n); // escreve na tela
    }
    
    close(fd);
    
    // remove a FIFO
    unlink(FIFO);
    
    return 0;
}
