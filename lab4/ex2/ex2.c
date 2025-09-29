#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

int main(int argc, char* argv[]) {
    int fd_in; /* descritor a ser duplicado */
    int fd_out; /* descritor a ser duplicado */
    int retdup1; /* valor de retorno de dup */
    int retdup2; /* valor de retorno de dup */

    if ((fd_in = open("entrada.txt", O_RDONLY)) == -1) {
        perror("failed to open entrada.txt");
        exit(1);
    }

    if ((fd_out = open("saida.txt", O_CREAT | O_WRONLY| O_TRUNC)) == -1) {
        perror("failed to open saida.txt");
        exit(1);
    }

    close(0); /* fechamento da entrada stdin */
    close(1); /* fechamento da saída stdout */
    
    if ((retdup1 = dup(fd_in)) == -1) { /* duplicacao de stdin (menor descritor fechado) */
        perror("failed to dup()");
        exit(2);
    }

    if ((retdup2 = dup(fd_out)) == -1) {
        perror("failed to dup");
        exit(3);
    }

    printf("valor de retorno de dup(): %d \n", retdup1);
    printf("valor de retorno de dup2(): %d \n", retdup2); 
    printf("a seguir, conteudo do stdin...\n");

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        // A função fputs escreve a string em um stream de saída.
        // Usamos stdout, que agora aponta para "saida.txt".
        fputs(buffer, stdout);
    }

    close(fd_in);
    close(fd_out);

    return 0;
}