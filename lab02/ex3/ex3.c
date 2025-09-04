#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int segmento;
    int *vetor;
    int tamanho = 20;
    int chave = 16;
    int num_processos = 4;
    int i, j, pid, status;
    
    segmento = shmget(IPC_PRIVATE, tamanho * sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); //cria shared memory
    vetor = (int *) shmat(segmento, 0, 0);
    
    for (i = 0; i < tamanho; i++) { //preencher vetor
        vetor[i] = rand() % 30;
        printf("%d ", vetor[i]);
    }
    printf("\n");
    
    printf("procurando: %d\n", chave);
    
    for (i = 0; i < num_processos; i++) {
        pid = fork();
        
        if (pid == 0) {
            int inicio = i * (tamanho / num_processos);
            int fim = inicio + (tamanho / num_processos);
            if (i == num_processos - 1) {
                fim = tamanho;
            }
            
            for (j = inicio; j < fim; j++) { //procurar
                if (vetor[j] == chave) {
                    printf("processo %d achou na pos %d\n", i+1, j);
                    exit(j);
                }
            }
            exit(-1);
        }
    }
    
    for (i = 0; i < num_processos; i++) {
        pid = wait(&status); //esperar filhos
    }
    
    shmdt(vetor);
    shmctl(segmento, IPC_RMID, 0);
    
    return 0;
}

