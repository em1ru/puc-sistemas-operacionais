#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int main() {
    key_t key = 4321;
    key_t semkey = 8765;
    int shmid = shmget(key, sizeof(int), 0666);
    int *valor = (int *)shmat(shmid, NULL, 0);
    int semid = semget(semkey, 1, 0666);
    for (int i = 0; i < 10; i++) {
        struct sembuf p = {0, -1, 0};
        struct sembuf v = {0, 1, 0};
        semop(semid, &p, 1);
        *valor += 5;
        printf("soma5: valor = %d\n", *valor);
        semop(semid, &v, 1);
        sleep(1);
    }
    shmdt(valor);
    return 0;
}
