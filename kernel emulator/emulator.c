#include "emulator.h"
#include "kernel.h"
#include "interController.h"
#include "genericProcess.h"

int main(int argc, char *argv[]) {
/* 
    O emulador inicia aqui. Ele dá origem ao kernel, ao controlador de interrupções 
    e aos processos A1, A2, A3, A4 e A5. Portanto, o número de processos EMUL_PROC_NUM é definido 
    como 7, e um fork é feito para gerar cada processo individualmente. Veja que o loop 
    começa em EMUL_PROC_NUM e vai até 0, para que o kernel possa receber uma lista com os PIDs 
    des processos genéricos.
*/
    pid_t pid;
    pid_t kernel_pid;
    pid_t procList[EMUL_PROC_NUM - 2]; // Lista de PIDs dos processos genéricos (A1 a A5)
    for (int i = EMUL_PROC_NUM; i >= 0; i++) 
    {
        if ((pid = fork()) == 0) 
        {
            switch (i) 
            {
                case 0:
                    kernelSim(procList);
                    exit(KERNEL_FINISH); 
                    break;
                case 1:
                    interControllerSim();
                    exit(INTERCONT_FINISH); 
                    break;
                case 2:
                    procList[0] = pid;
                    genericProcess(); // A1
                    exit(GENPROC_FINISH); 
                    break;
                case 3:
                    procList[1] = pid;
                    genericProcess(); // A2
                    exit(GENPROC_FINISH);
                    break;
                case 4:
                    procList[2] = pid;
                    genericProcess(); // A3
                    exit(GENPROC_FINISH);
                    break;
                case 5:
                    procList[3] = pid;
                    genericProcess(); // A4
                    exit(GENPROC_FINISH);
                    break;
                case 6:
                    procList[4] = pid;
                    genericProcess(); // A5
                    exit(GENPROC_FINISH);
                    break;
            }
            return 0; // Processo filho termina aqui
        }

        // Caso haja erro ao criar algum dos processos
        else if (pid < 0) 
        {
            perror("Fork %d failed", i);
            exit(FORK_ERROR);
        }
    }

    // Processo pai (emulador) espera todos os processos filhos terminarem para encerrar
    waitpid(-1, NULL, 0);

    return 0;    
}