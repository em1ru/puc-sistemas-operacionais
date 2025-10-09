// Exercicio 1 - Programa que le do teclado e escreve na FIFO
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define FIFO "minhaFifo"

int main() {
    int fd;
    char msg[256];
    
    // cria a FIFO se nao existe
    if (access(FIFO, F_OK) == -1) {
        if (mkfifo(FIFO, S_IRUSR | S_IWUSR) == -1) {
            perror("mkfifo");
            exit(1);
        }
    }
    
    printf("Digite mensagens (Ctrl+D para sair):\n");
    
    // le do teclado e escreve na FIFO
    while (fgets(msg, sizeof(msg), stdin) != NULL) {
        fd = open(FIFO, O_WRONLY);
        if (fd == -1) {
            perror("open");
            exit(1);
        }
        
        write(fd, msg, strlen(msg));
        close(fd);
    }
    
    printf("\nEncerrando...\n");
    return 0;
}
