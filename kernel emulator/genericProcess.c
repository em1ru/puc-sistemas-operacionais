#include "genericProcess.h"
#include "emulator.h"

extern pid_t kernel_pid;

static int my_id = 0;
static int my_pipe = 0;

void genericProcess(int process_id, int pipe_to_kernel, int pipe_from_kernel) {
    int i, j;
    char msg[256];
    
    my_id = process_id;
    my_pipe = pipe_to_kernel;
    
    printf("[A%d] Iniciado (PID %d)\n", my_id, getpid());
    
    srand(time(NULL) + getpid());
    
    // espera ser escalonado
    kill(getpid(), SIGSTOP);
    
    // faz o trabalho
    for (i = 0; i < MAX_ITERATIONS; i++) {
        printf("[A%d] Iteracao %d/%d\n", my_id, i + 1, MAX_ITERATIONS);
        
        // trabalho simulado
        volatile long sum = 0;
        for (j = 0; j < 1000000; j++) {
            sum += j * i;
        }
        
        // as vezes faz I/O
        if (rand() % 100 < 20) {
            int dev = (rand() % 2) + 1;
            char op = (rand() % 2 == 0) ? 'R' : 'W';
            
            printf("[A%d] Fazendo I/O: D%d, op=%c\n", my_id, dev, op);
            
            snprintf(msg, sizeof(msg), "IO:%d:%d:%c", my_id, dev, op);
            write(pipe_to_kernel, msg, strlen(msg) + 1);
            kill(kernel_pid, SIG_IO);
            
            printf("[A%d] Aguardando I/O...\n", my_id);
            pause();
        }
        
        sleep(1);
    }
    
    printf("[A%d] Finalizando\n", my_id);
}