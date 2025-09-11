#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h> // Para sprintf
#include <stdlib.h> // Para sprintf

#define SEQ_INITIAL_FLAG 0
#define TRUE 1

typedef struct {
    int num;
    int seq;
} Dado;

Dado dado = {0, SEQ_INITIAL_FLAG};

int main(void) {
    // criando e atribuindo as shared memories 
    int id_m1 = shmget(IPC_PRIVATE, sizeof(Dado), IPC_CREAT | 0666);
    int id_m2 = shmget(IPC_PRIVATE, sizeof(Dado), IPC_CREAT | 0666);
    // Dado *pagina1 = (Dado*) shmat(id_m1, NULL, 0);
    // Dado *pagina2 = (Dado*) shmat(id_m2, NULL, 0);

    // "Attach" da memória compartilhada
    Dado *pagina1 = (Dado*) shmat(id_m1, NULL, 0);
    
    // --- VERIFICAÇÃO ESSENCIAL ---
    if (pagina1 == (void*) -1) {
        perror("pai.c: shmat para pagina1 falhou"); // perror diz o motivo do erro
        exit(1); // Aborta o programa se a memória não puder ser usada
    }

    Dado *pagina2 = (Dado*) shmat(id_m2, NULL, 0);

    // --- VERIFICAÇÃO ESSENCIAL ---
    if (pagina2 == (void*) -1) {
        perror("pai.c: shmat para pagina2 falhou");
        exit(1);
    }

    // inicializa a shared memory
    *pagina1 = dado;
    *pagina2 = dado;

    // criar dois filhos
    for (int i = 0; i < 2; i++) {
        int pid = fork();

        if (pid < 0) {
            // erro no fork, não gerou filho
            exit(1);
        }
        else if (pid == 0) {
            //filho

            // Um espaço na memória para guardar a string do id de memória
            char buffer_string[20]; 
            char numero_filho_str[5];  // buffer para o numero do filho

            sprintf(numero_filho_str, "%d", i);

            if (i == 0) {
                // filho 1
                sprintf(buffer_string, "%d", id_m1);
                execlp("./filho", "filho", buffer_string, numero_filho_str, (char*) NULL);
            }
            else {
                // filho 2
                sprintf(buffer_string, "%d", id_m2);
                execlp("./filho", "filho", buffer_string, numero_filho_str, (char*) NULL);
            }
        }
    }

    int produto = 1;

    int ultimo_seq1_lido = SEQ_INITIAL_FLAG;
    int ultimo_seq2_lido = SEQ_INITIAL_FLAG;

    while (TRUE) {
        if (pagina1->seq > ultimo_seq1_lido) {
            produto *= pagina1->num;
            ultimo_seq1_lido++;
        }
        if (pagina2->seq > ultimo_seq2_lido) {
            produto *= pagina2->num;
            ultimo_seq2_lido++;
        }
        if (ultimo_seq1_lido == 1 && ultimo_seq2_lido == 1) {
            break;
        }
    }

    printf("produto = %d\n", produto);
    
    // liberando a memória compartilhada 
    shmdt(pagina1);
    shmdt(pagina2);
    shmctl(id_m1, IPC_RMID, 0);
    shmctl(id_m2, IPC_RMID, 0);

    return 0;
}