#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int main(void) {
    pid_t filhos[3];
    char *progs[3] = {"./io1", "./io2", "./io3"};

    for (int i = 0; i < 3; i++) {
        if ((filhos[i] = fork()) == 0) {
            execl(progs[i], progs[i], NULL);
            exit(1);
        }
    }

    // Pausa todos os filhos logo após criar
    for (int i = 0; i < 3; i++) {
        kill(filhos[i], SIGSTOP);
    }

    printf("[pai] Escalonador iniciado. PIDs: %d %d %d\n",
           filhos[0], filhos[1], filhos[2]);

    int turno = 0;
    while (1) {
        // acorda o filho da vez
        kill(filhos[turno], SIGCONT);

        if (turno == 0)
            sleep(1); // quantum do primeiro filho
        else
            sleep(2); // quantum dos outros filhos

        // pausa o filho da vez
        kill(filhos[turno], SIGSTOP);

        // passa para o próximo (0→1→2→0...)
        turno = (turno + 1) % 3;
    }

    // nunca chega aqui, mas por formalidade
    for (int i = 0; i < 3; i++) {
        kill(filhos[i], SIGKILL);
    }
    return 0;
}
