#include <sys/shm.h>
#include <sys/stat.h>
#include <stdio.h>

#define SHM_ALREADY_CREATED 0

int main(void) {
    int id = 8752;
    int shm = shmget(id, SHM_ALREADY_CREATED, S_IRUSR);
    char* page = (char*)shmat(shm, NULL, 0);
    printf("cliente lê: %s\n", page);  
    
    shmdt(page);
    shmctl(shm, IPC_RMID, 0);
    
    return 0;
}