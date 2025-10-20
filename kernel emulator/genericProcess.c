#include "genericProcess.h"
#include "emulator.h"

extern pid_t kernel_pid;

static int my_id = 0;
static int my_pipe = 0;

void genericProcess(int process_id, int pipe_to_kernel, int pc_value_pipe, pid_t kernel_pid) {
    int program_counter, j;
    char msg[256];
    int d1_access_count = 0; 
    int d2_access_count = 0; 
    
    my_id = process_id;
    my_pipe = pipe_to_kernel;
    
    printf("[A%d] Iniciado (PID %d)\n", my_id, getpid());
    
    srand(time(NULL) + getpid());
    
    // espera ser escalonado
    kill(getpid(), SIGSTOP);
    
    // faz o trabalho
    for (program_counter = 0; program_counter < MAX_ITERATIONS; program_counter++) {
        write(pc_value_pipe, &program_counter, sizeof(program_counter)); // salva PC pro kernel poder consultar depois 
        printf("[A%d] Iteracao %d/%d\n", my_id, program_counter + 1, MAX_ITERATIONS);
        
        // probabilidade de 10% de fazer I/O
        if (rand() % 100 < 10) {
            int dev = (rand() % 2) + 1; // meio a meio pros devices
            char op = (rand() % 2 == 0) ? 'R' : 'W'; // meio a meio pras operações
            
            // Incrementa contadores de acesso
            if (dev == 1) {
                d1_access_count++;
            } else {
                d2_access_count++;
            }

            printf("[A%d] Fazendo I/O: D%d, op=%c\n", my_id, dev, op);
            
            snprintf(msg, sizeof(msg), "IO:%d:%d:%c:%d:%d", my_id, dev, op, d1_access_count, d2_access_count); // salva mensagem no buffer msg
            write(pipe_to_kernel, msg, strlen(msg) + 1); // escreve mensagem do buffer msg na pipe pro kernel
            printf("[A%d] Realizando I/O (%c)...\n", my_id, op);
            kill(kernel_pid, SIG_IO); // avisa ao kernel que fez I/O
        }
        else {
            // trabalho simulado qualquer sem ser I/O
            long sum = 0;
            for (j = 0; j < 1000000; j++) {
                sum += 1;
            }
        }
        
        sleep(1);
    }
    
    printf("[A%d] Finalizando\n", my_id);
}