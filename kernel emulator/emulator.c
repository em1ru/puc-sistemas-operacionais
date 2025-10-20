#include "emulator.h"
#include "kernel.h"
#include "interruptionController.h"
#include "genericProcess.h"

int pipe_to_kernel[2]; // pipe que processos genéricos usam para notificar kernel de I/O
int intercontrol_kernel_pipe[2]; // pipe pro intercontroller notificar o kernel do fim de I/O nos devices 
pid_t intercontroller_pid;

// array de pipes para os program counters
int pc_pipes[NUM_PROCESSES][2];

int main(int argc, char *argv[]) {
    pid_t pid;
    pid_t kernel_pid;
    pid_t process_list[NUM_PROCESSES];
    int i;
    int pc_pipes_read_ends[NUM_PROCESSES]; // Apenas para passar os descritores de leitura para o kernel
    
    printf("=== Simulador de SO ===\n\n");
    
    // cria pipes
    if (pipe(pipe_to_kernel) == -1 || pipe(intercontrol_kernel_pipe) == -1) {
        printf("Erro ao criar pipes\n");
        exit(FORK_ERROR);
    }

    // criar pipes de PC
    for (i = 0; i < NUM_PROCESSES; i++) {
        if (pipe(pc_pipes[i]) == -1) {
            printf("Erro ao criar pipe de PC para o processo A%d\n", i + 1);
            exit(FORK_ERROR);
        }
        pc_pipes_read_ends[i] = pc_pipes[i][0];
    }

    // criar processos A1 a A5
    for (i = 0; i < NUM_PROCESSES; i++) {
        printf("Criando processo A%d...\n", i + 1);
        
        pid = fork();
        if (pid == 0) {
            close(pipe_to_kernel[0]);
            close(intercontrol_kernel_pipe[0]);
            close(intercontrol_kernel_pipe[1]);

            // Passar o pipe de PC para o processo
            close(pc_pipes[i][0]); // Processo só escreve
            for (int j = 0; j < NUM_PROCESSES; j++) {
                if (i != j) {
                   close(pc_pipes[j][0]);
                   close(pc_pipes[j][1]);
                }
            }
            genericProcess(i + 1, pipe_to_kernel[1], pc_pipes[i][1], kernel_pid);

            exit(GENPROC_FINISH);
        } else if (pid < 0) {
            printf("Erro ao criar processo A%d\n", i + 1);
            exit(FORK_ERROR);
        }
        
        process_list[i] = pid;
        printf("A%d criado (PID %d)\n", i + 1, pid);
    }

        // criar kernel
    printf("Criando KernelSim...\n");
    pid = fork();
    if (pid == 0) {
        close(pipe_to_kernel[1]); // kernel lê
        close(intercontrol_kernel_pipe[1]); // kernel lê
        
        // Passar os pipes de PC para o kernel
        for (i = 0; i < NUM_PROCESSES; i++) {
            close(pc_pipes[i][1]); // Kernel só lê
        }
        // Note: A lista de processos ainda está vazia aqui, o kernel não deve usá-la na inicialização
        kernelSim(process_list, pipe_to_kernel[0], intercontrol_kernel_pipe[0], pc_pipes_read_ends);

        exit(KERNEL_FINISH);
    } else if (pid < 0) {
        printf("Erro no fork do kernel\n");
        exit(FORK_ERROR);
    }
    kernel_pid = pid;
    printf("KernelSim criado (PID %d)\n", kernel_pid);

    // criar controlador de interrupcoes
    printf("Criando InterControllerSim...\n");
    pid = fork();
    if (pid == 0) {
        close(intercontrol_kernel_pipe[0]); // fecha leitura porque ele apenas escreve pro kernel
        interControllerSim(intercontrol_kernel_pipe[1], kernel_pid); // passa a entrada da pipe de escrita pro intercontroller
        exit(INTERCONT_FINISH);
    } else if (pid < 0) {
        printf("Erro ao criar InterController\n");
        exit(FORK_ERROR);
    }
    intercontroller_pid = pid;
    printf("InterControllerSim criado (PID %d)\n\n", intercontroller_pid);
    
    // Fechar todos os pipes no processo pai (emulador)
    close(pipe_to_kernel[0]);
    close(pipe_to_kernel[1]);
    close(intercontrol_kernel_pipe[0]);
    close(intercontrol_kernel_pipe[1]);
    for (i = 0; i < NUM_PROCESSES; i++) {
        close(pc_pipes[i][0]);
        close(pc_pipes[i][1]);
    }

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