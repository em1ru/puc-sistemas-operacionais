#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char* argv[]) {
    int pid;
    int fd[2]; // fd[0] leitura, fd[1] escrita
    int readDataSize, writtenDataSize;

    
    char writeBuffer[] = "mensagem do pai pro filho"; 
    int bufferSize = strlen(writeBuffer) + 1;
    char* readBuffer = (char*)malloc(bufferSize);

    if (pipe(fd) < 0) {
        perror("failed to open pipe");
        exit(-1);
    }

    pid = fork();

    if (pid < 0) {
        perror("failed to fork");
        exit(1);
    }
    else if (pid > 0){ //pai
        close(fd[0]); // pai não lê
        writtenDataSize = write(fd[1], writeBuffer, bufferSize);
        printf("[pai] %d dados escritos\n", writtenDataSize);
    }
    else { // filho
        close(fd[1]); // filho não escreve
        readDataSize = read(fd[0], readBuffer, bufferSize);
        printf("[filho] %d dados lidos: %s\n", readDataSize, readBuffer);
    }

    return 0;
}