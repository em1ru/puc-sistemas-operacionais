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
    int shmid = shmget(key, sizeof(struct shm), IPC_CREAT | 0666);
    struct shm *mem = (struct shm *)shmat(shmid, NULL, 0);
    int semid = semget(semkey, 2, IPC_CREAT | 0666);
    union semun sem_union;
    sem_union.val = 1;
    semctl(semid, 0, SETVAL, sem_union); // mutex
    sem_union.val = 0;
    semctl(semid, 1, SETVAL, sem_union); // msg pronta
    char input[MSG_SIZE];
    while (1) {
        printf("Digite mensagem: ");
        fflush(stdout);
        if (fgets(input, MSG_SIZE, stdin) == NULL) {
            break;
        }
        struct sembuf p_mutex = {0, -1, 0};
        struct sembuf v_mutex = {0, 1, 0};
        struct sembuf v_msg = {1, 1, 0};
        semop(semid, &p_mutex, 1);
        strncpy(mem->msg, input, MSG_SIZE);
        semop(semid, &v_mutex, 1);
        semop(semid, &v_msg, 1);
        if (strncmp(input, "fim", 3) == 0) {
            break;
        }
    }
    shmdt(mem);
    return 0;
}
