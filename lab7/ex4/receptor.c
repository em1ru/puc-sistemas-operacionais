#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>

#define MSG_SIZE 64

struct shm {
    char msg[MSG_SIZE];
};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int main() {
    key_t key = 2468;
    key_t semkey = 1357;
    int shmid = shmget(key, sizeof(struct shm), 0666);
    struct shm *mem = (struct shm *)shmat(shmid, NULL, 0);
    int semid = semget(semkey, 2, 0666);
    while (1) {
        struct sembuf p_msg = {1, -1, 0};
        struct sembuf p_mutex = {0, -1, 0};
        struct sembuf v_mutex = {0, 1, 0};
        semop(semid, &p_msg, 1);
        semop(semid, &p_mutex, 1);
        printf("Recebido: %s", mem->msg);
        semop(semid, &v_mutex, 1);
        if (strncmp(mem->msg, "fim", 3) == 0) break;
    }
    shmdt(mem);
    return 0;
}
