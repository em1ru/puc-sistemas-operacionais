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
    
    shmid = shmget(key, sizeof(struct shm), 0666);
    mem = (struct shm *)shmat(shmid, NULL, 0);
    semid = semget(semkey, 2, 0666);
    
    while (1) {
        struct sembuf p_cheio = {1, -1, 0};
        struct sembuf p_mutex = {0, -1, 0};
        struct sembuf v_mutex = {0, 1, 0};
        semop(semid, &p_cheio, 1);
        semop(semid, &p_mutex, 1);
        for (int i = 0; i < BUF_SIZE; i++) {
            putchar(mem->buffer[i]);
        }
        fflush(stdout);
        semop(semid, &v_mutex, 1);
    }
    shmdt(mem);
    return 0;
}
