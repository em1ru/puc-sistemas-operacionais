// Exercicio 3 - Cliente que envia mensagens ao servidor
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define FIFO_CLIENT_TO_SERVER "fifo_c2s"
#define FIFO_SERVER_TO_CLIENT "fifo_s2c"

int main() {
    int fd_write, fd_read;
    char msg[256];
    char resposta[256];
    int n;
    
    // verifica se as FIFOs existem
    if (access(FIFO_CLIENT_TO_SERVER, F_OK) == -1) {
        printf("Erro: servidor nao esta rodando\n");
        exit(1);
    }
    
    printf("Cliente conectado. Digite mensagens:\n");
    
    // le mensagens do teclado
    while (fgets(msg, sizeof(msg), stdin) != NULL) {
        // abre FIFO para escrever pro servidor
        fd_write = open(FIFO_CLIENT_TO_SERVER, O_WRONLY);
        if (fd_write == -1) {
            perror("open write");
            exit(1);
        }
        
        // envia mensagem
        write(fd_write, msg, strlen(msg));
        close(fd_write);
        
        // abre FIFO para ler resposta do servidor
        fd_read = open(FIFO_SERVER_TO_CLIENT, O_RDONLY);
        if (fd_read == -1) {
            perror("open read");
            exit(1);
        }
        
        // le resposta
        n = read(fd_read, resposta, sizeof(resposta) - 1);
        if (n > 0) {
            resposta[n] = '\0';
            printf("Resposta: %s", resposta);
        }
        
        close(fd_read);
    }
    
    printf("\nEncerrando cliente...\n");
    return 0;
}
