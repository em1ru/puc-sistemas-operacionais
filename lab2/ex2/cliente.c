#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main() {
    int segmento;
    char *p;
    
    printf("Digite o ID do segmento (mostrado pelo servidor): ");
    scanf("%d", &segmento);
    
    p = (char *) shmat(segmento, 0, 0); //conecta com a shared memory
    
    printf("A mensagem do dia é: %s\n", p);
    
    shmdt(p);
    
    shmctl(segmento, IPC_RMID, 0);
    
    return 0;
}
