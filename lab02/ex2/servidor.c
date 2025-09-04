#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

int main() {
    int segmento;
    char *p;
    char mensagem[1024];
    
    segmento = shmget(IPC_PRIVATE, 1024, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    
    printf("Memória criada com ID: %d\n", segmento);
    
    p = (char *) shmat(segmento, 0, 0); //attach
    
    printf("Digite mensagem do dia: ");
    scanf("%s", mensagem);
    
    int i = 0;
    while (mensagem[i] != '\0') {
        p[i] = mensagem[i]; //salva msg na shared memory
        i++;
    }
    p[i] = '\0';
    
    printf("mensagem salva!\n");
    
    shmdt(p);
    
    return 0;
}
