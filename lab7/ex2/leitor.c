#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>

#define BUF_SIZE 16

struct shm {
    char buffer[BUF_SIZE];
    int pos;
};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int main() {
    int shmid, semid;
    key_t key = 1234;
    key_t semkey = 5678;
    struct shm *mem;
    union semun sem_union;
    
    shmid = shmget(key, sizeof(struct shm), IPC_CREAT | 0666);
    mem = (struct shm *)shmat(shmid, NULL, 0);
    semid = semget(semkey, 2, IPC_CREAT | 0666);
    
    sem_union.val = 1;
    semctl(semid, 0, SETVAL, sem_union); // mutex
    sem_union.val = 0;
    semctl(semid, 1, SETVAL, sem_union); // cheio
    
    mem->pos = 0;
    while (1) {
        char c = getchar();
        struct sembuf p_mutex = {0, -1, 0};
        struct sembuf v_mutex = {0, 1, 0};
        struct sembuf v_cheio = {1, 1, 0};
        semop(semid, &p_mutex, 1);
        mem->buffer[mem->pos] = c;
        mem->pos++;
        if (mem->pos == BUF_SIZE) {
            semop(semid, &v_cheio, 1);
            mem->pos = 0;
        }
        semop(semid, &v_mutex, 1);
        if (c == EOF) break;
    }
    shmdt(mem);
    return 0;
}
