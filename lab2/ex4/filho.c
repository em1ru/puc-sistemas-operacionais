#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h> // Para rand() e srand()
#include <sys/wait.h>
#include <time.h>

#define NUM_FILHO_1 3
#define NUM_FILHO_2 8
#define SEQ_INITIAL_FLAG 0


typedef struct {
    int num;
    int seq;
} Dado;

int main(int argc, char* argv[]) {
    int id = atoi(argv[1]); // recebe o id do processo pai
    int num_filho = atoi(argv[2]);
    Dado* pagina = (Dado*) shmat(id, NULL, 0); // attach

    // Gera um tempo de espera entre 500,000 e 1,499,999 microssegundos (0.5 a 1.5 segundos)
    srand(time(NULL) * getpid()); // seed única para cada filho
    int tempo_em_microssegundos = (rand() % 1000000) + 500000;
    usleep(tempo_em_microssegundos);

    Dado dado;

    // por filho
    if (num_filho == 0) {
        dado.num = NUM_FILHO_1; 
        dado.seq = SEQ_INITIAL_FLAG + 1;
        *pagina = dado;
    }
    else {
        dado.num = NUM_FILHO_2; 
        dado.seq = SEQ_INITIAL_FLAG + 1;
        *pagina = dado;
    }

    shmdt(pagina);
    return 0;
}