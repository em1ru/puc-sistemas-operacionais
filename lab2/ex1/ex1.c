#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// #include <stdlib.h>

#define NUM_LINHAS 3
#define NUM_COLUNAS 3

#define EXIT_SUCCESS 2
#define EXIT_ERROR 1

typedef int Matriz[NUM_LINHAS * NUM_COLUNAS];

int main(void) {
    int pid, status;

    // matrizes mockada 3 x 3
    const Matriz m1 = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    const Matriz m2 = {9, 9, 9, 8, 8, 8, 7, 7, 7};
    const Matriz m3 = {0, 0, 0, 0, 0, 0, 0, 0, 0};

    int shm1 = shmget(IPC_PRIVATE, sizeof(Matriz), IPC_CREAT | 0666);
    int shm2 = shmget(IPC_PRIVATE, sizeof(Matriz), IPC_CREAT | 0666);
    int shm3 = shmget(IPC_PRIVATE, sizeof(Matriz), IPC_CREAT | 0666);

    int* pg1 = (int*)shmat(shm1, NULL, 0);
    int* pg2 = (int*)shmat(shm2, NULL, 0);
    int* pg3 = (int*)shmat(shm3, NULL, 0);

    // copiando conteúdo das matrizes nas memórias compartilhadas
    memcpy(pg1, m1, sizeof(Matriz));
    memcpy(pg2, m2, sizeof(Matriz));
    memcpy(pg3, m3, sizeof(Matriz));

    for (int i = 0; i < NUM_LINHAS; i++) {
        // tantos filhos quanto for o número de linhas
        pid = fork();

        if (pid < 0) {
            exit(EXIT_ERROR); // erro no fork
        }
        else if (pid == 0) {
            // filho 
            for (int pos = i*NUM_COLUNAS; pos < i*NUM_COLUNAS + NUM_COLUNAS; pos++) {
                // somar elementos da i-ésima linha de m1 e m2
                pg3[pos] = pg1[pos] + pg2[pos];
            }
            exit(EXIT_SUCCESS);
        }
    }

    //paizão espera filhos e depois imprime resultado
    while((pid = wait(&status)) > 0);

    printf("Resultado Matriz 3:\n");
    for (int j = 0; j < NUM_LINHAS; j++) {
        for (int k = 0; k < NUM_COLUNAS; k++) {
            printf("%d  ", pg3[j*NUM_COLUNAS + k]);
        }
        printf("\n");
    }

    // dettach e liberando memórias
    shmdt(pg1);
    shmdt(pg2);
    shmdt(pg3);

    shmctl(shm1, IPC_RMID, 0);
    shmctl(shm2, IPC_RMID, 0);
    shmctl(shm3, IPC_RMID, 0);

    return 0;
}