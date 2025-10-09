// Exercicio 1 - Programa que le da FIFO e mostra na tela
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define FIFO "minhaFifo"

int main() {
    int fd;
    char ch;
    
    // cria a FIFO se nao existe
    if (access(FIFO, F_OK) == -1) {
        if (mkfifo(FIFO, S_IRUSR | S_IWUSR) == -1) {
            perror("mkfifo");
            exit(1);
        }
        printf("FIFO criada\n");
    }
    
    printf("Aguardando mensagens...\n");
    
    // loop infinito lendo da FIFO
    while (1) {
        fd = open(FIFO, O_RDONLY);
        if (fd == -1) {
            perror("open");
            exit(1);
        }
        
        // le caractere por caractere e mostra na tela
        while (read(fd, &ch, sizeof(ch)) > 0) {
            putchar(ch);
        }
        
        close(fd);
    }
    
    return 0;
}
