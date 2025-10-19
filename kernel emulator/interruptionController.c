#include "interruptionController.h"
#include "emulator.h"

extern pid_t kernel_pid;

void interControllerSim(int pipe_to_kernel) {
    int count = 0;
    int max_iter = MAX_ITERATIONS * NUM_PROCESSES * 2;
    
    printf("[INTERCONTROLLER] Iniciado (PID %d)\n", getpid());
    printf("[INTERCONTROLLER] Monitorando kernel %d\n\n", kernel_pid);
    
    srand(time(NULL));
    
    while (count < max_iter) {
        sleep(TIME_QUANTUM);
        
        // manda interrupcao de timer
        if (count % 2 == 0) {
            printf("[INTERCONTROLLER] Enviando INT timer\n");
            kill(kernel_pid, SIG_TIMER);
        }
        
        // as vezes manda I/O
        if (rand() % 100 < 30) {
            sleep(1);
            printf("[INTERCONTROLLER] Enviando INT I/O\n");
            kill(kernel_pid, SIG_IO);
        }
        
        count++;
    }
    
    printf("\n[INTERCONTROLLER] Finalizando\n");
}

