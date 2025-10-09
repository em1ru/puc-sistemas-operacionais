// Exercicio 3 - Servidor que converte mensagens para MAIUSCULAS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#define FIFO_CLIENT_TO_SERVER "fifo_c2s"
#define FIFO_SERVER_TO_CLIENT "fifo_s2c"

int main() {
    int fd_read, fd_write;
    char buffer[256];
    int n, i;
    
    // cria as FIFOs se nao existem
    if (access(FIFO_CLIENT_TO_SERVER, F_OK) == -1) {
        if (mkfifo(FIFO_CLIENT_TO_SERVER, S_IRUSR | S_IWUSR) == -1) {
            perror("mkfifo cliente->servidor");
            exit(1);
        }
    }
    
    if (access(FIFO_SERVER_TO_CLIENT, F_OK) == -1) {
        if (mkfifo(FIFO_SERVER_TO_CLIENT, S_IRUSR | S_IWUSR) == -1) {
            perror("mkfifo servidor->cliente");
            exit(1);
        }
    }
    
    printf("Servidor rodando...\n");
    
    // loop infinito esperando mensagens
    while (1) {
        // abre FIFO para ler do cliente
        fd_read = open(FIFO_CLIENT_TO_SERVER, O_RDONLY);
        if (fd_read == -1) {
            perror("open read");
            exit(1);
        }
        
        // le mensagem do cliente
        n = read(fd_read, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            printf("Recebido: %s", buffer);
            
            // converte para maiusculas
            for (i = 0; i < n; i++) {
                buffer[i] = toupper(buffer[i]);
            }
            
            printf("Enviando: %s", buffer);
            
            // abre FIFO para escrever resposta
            fd_write = open(FIFO_SERVER_TO_CLIENT, O_WRONLY);
            if (fd_write == -1) {
                perror("open write");
                close(fd_read);
                continue;
            }
            
            // envia resposta
            write(fd_write, buffer, n);
            close(fd_write);
        }
        
        close(fd_read);
    }
    
    return 0;
}
