#include "emulator.h"
#include "kernel.h"
#include "interruptionController.h"
#include "genericProcess.h"

int pipe_to_kernel[2];
int pipe_from_kernel[2];
pid_t kernel_pid;
pid_t intercontroller_pid;

int main(int argc, char *argv[]) {
    pid_t pid;
    pid_t process_list[NUM_PROCESSES];
    int i;
    
    printf("=== Simulador de SO ===\n\n");
    
    // cria pipes
    if (pipe(pipe_to_kernel) == -1 || pipe(pipe_from_kernel) == -1) {
        printf("Erro ao criar pipes\n");
        exit(FORK_ERROR);
    }
    
    // criar kernel
    printf("Criando KernelSim...\n");
    pid = fork();
    if (pid == 0) {
        close(pipe_to_kernel[1]);
        close(pipe_from_kernel[0]);
        kernelSim(process_list, pipe_to_kernel[0], pipe_from_kernel[1]);
        exit(KERNEL_FINISH);
    } else if (pid < 0) {
        printf("Erro no fork do kernel\n");
        exit(FORK_ERROR);
    }
    kernel_pid = pid;
    printf("KernelSim criado (PID %d)\n", kernel_pid);
    sleep(1);
    
    // criar processos A1 a A5
    for (i = 0; i < NUM_PROCESSES; i++) {
        printf("Criando processo A%d...\n", i + 1);
        
        pid = fork();
        if (pid == 0) {
            close(pipe_to_kernel[0]);
            close(pipe_from_kernel[1]);
            genericProcess(i + 1, pipe_to_kernel[1], pipe_from_kernel[0]);
            exit(GENPROC_FINISH);
        } else if (pid < 0) {
            printf("Erro ao criar processo A%d\n", i + 1);
            exit(FORK_ERROR);
        }
        
        process_list[i] = pid;
        printf("A%d criado (PID %d)\n", i + 1, pid);
    }
    
    sleep(1);
    
    // criar controlador de interrupcoes
    printf("Criando InterControllerSim...\n");
    pid = fork();
    if (pid == 0) {
        interControllerSim(pipe_to_kernel[1]);
        exit(INTERCONT_FINISH);
    } else if (pid < 0) {
        printf("Erro ao criar InterController\n");
        exit(FORK_ERROR);
    }
    intercontroller_pid = pid;
    printf("InterControllerSim criado (PID %d)\n\n", intercontroller_pid);
    
    close(pipe_to_kernel[0]);
    close(pipe_to_kernel[1]);
    close(pipe_from_kernel[0]);
    close(pipe_from_kernel[1]);
    
    printf("Sistema rodando...\n\n");
    
    // espera todos terminarem
    int status;
    int finished = 0;
    int total = NUM_PROCESSES + 2;
    
    while (finished < total) {
        pid_t p = wait(&status);
        if (p > 0) {
            finished++;
        }
    }
    
    printf("\nTodos os processos finalizaram\n");
    
    return 0;
}