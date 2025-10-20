#include "interruptionController.h"
#include "emulator.h"

extern pid_t kernel_pid;

void interControllerSim(int pipe_to_kernel, pid_t kernel_pid) {
    int prob; // variável auxiliar pra verificar probabilidade de liberar algum device de I/O
    int device; // guarda o número do device a ser liberado
    int count = 0;
    int max_iter = MAX_ITERATIONS * NUM_PROCESSES * TIME_QUANTUM;
    char msg[256];
    
    printf("[INTERCONTROLLER] Iniciado (PID %d)\n", getpid());
    printf("[INTERCONTROLLER] Monitorando kernel %d\n\n", kernel_pid);
    
    srand(time(NULL));
    
    while (count < max_iter) {
        sleep(TIME_QUANTUM);
        
        // manda interrupcao de timer a cada quantum de tempo
        printf("[INTERCONTROLLER] Enviando INT timer\n");
        kill(kernel_pid, SIG_TIMER);
        
        // às vezes manda I/O
        if (prob = rand() % 100 < 20) { // com 20% de chance... 

            device = (prob == 0) ? 2 : 1; // com 1% de chance, libera o D2; caso contrário, libera D1 com 20% de chance
            snprintf(msg, sizeof(msg), "IRQ%d", device); // salva mensagem no buffer msg (IRQ1 ou IRQ2)
            write(pipe_to_kernel, msg, strlen(msg) + 1); // escreve mensagem do buffer msg na pipe pro kernel
            printf("[INTERCONTROLLER] Interrompendo I/O em D%d\n", device);
            kill(kernel_pid, SIG_IO);
        }
        
        count++;
    }
    
    printf("\n[INTERCONTROLLER] Finalizando\n");
}

