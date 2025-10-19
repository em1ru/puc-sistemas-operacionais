#include "kernel.h"
#include "emulator.h"

static ProcessControlBlock pcb_table[NUM_PROCESSES];
static int current_process = -1;
static int ready_queue[NUM_PROCESSES];
static int ready_count = 0;
static int pipe_input;
static int pipe_output;
static volatile sig_atomic_t timer_interrupt = 0;
static volatile sig_atomic_t io_interrupt = 0;
static int running = 1;

void timerInterruptHandler(int sig) {
    timer_interrupt = 1;
    printf("\n[KERNEL] Interrupcao de timer\n");
}

void ioInterruptHandler(int sig) {
    io_interrupt = 1;
    printf("\n[KERNEL] Interrupcao de I/O\n");
}

void blockProcess(pid_t pid, int device) {
    int i;
    for (i = 0; i < NUM_PROCESSES; i++) {
        if (pcb_table[i].pid == pid) {
            pcb_table[i].state = BLOCKED;
            pcb_table[i].blocked_device = device;
            kill(pid, SIGSTOP);
            printf("[KERNEL] A%d bloqueado para I/O (D%d)\n", pcb_table[i].id, device);
            return;
        }
    }
}

void unblockProcess(pid_t pid) {
    int i;
    for (i = 0; i < NUM_PROCESSES; i++) {
        if (pcb_table[i].pid == pid && pcb_table[i].state == BLOCKED) {
            pcb_table[i].state = READY;
            pcb_table[i].blocked_device = 0;
            printf("[KERNEL] A%d desbloqueado\n", pcb_table[i].id);
            ready_queue[ready_count++] = i;
            return;
        }
    }
}

void roundRobinScheduling() {
    int i;
    
    // para processo atual
    if (current_process >= 0 && pcb_table[current_process].state == RUNNING) {
        kill(pcb_table[current_process].pid, SIGSTOP);
        
        if (pcb_table[current_process].state == RUNNING) {
            pcb_table[current_process].state = READY;
            ready_queue[ready_count++] = current_process;
            printf("[KERNEL] A%d preemptado\n", pcb_table[current_process].id);
        }
    }
    
    // pega proximo da fila
    if (ready_count > 0) {
        current_process = ready_queue[0];
        
        // remove da fila
        for (i = 0; i < ready_count - 1; i++) {
            ready_queue[i] = ready_queue[i + 1];
        }
        ready_count--;
        
        pcb_table[current_process].state = RUNNING;
        printf("[KERNEL] Escalonando A%d\n", pcb_table[current_process].id);
        kill(pcb_table[current_process].pid, SIGCONT);
    } else {
        current_process = -1;
        printf("[KERNEL] Nenhum processo pronto (IDLE)\n");
    }
}

void kernelSim(pid_t process_list[], int pipe_in, int pipe_out) {
    int i;
    char buffer[256];
    int quantum_counter = 0;
    int total_time = 0;
    int max_time = MAX_ITERATIONS * TIME_QUANTUM * NUM_PROCESSES;
    
    printf("[KERNEL] Iniciado (PID %d)\n", getpid());
    
    pipe_input = pipe_in;
    pipe_output = pipe_out;
    
    // inicializa PCBs
    for (i = 0; i < NUM_PROCESSES; i++) {
        pcb_table[i].pid = process_list[i];
        pcb_table[i].id = i + 1;
        pcb_table[i].state = READY;
        pcb_table[i].blocked_device = 0;
        ready_queue[i] = i;
        ready_count++;
    }
    
    // configura handlers
    signal(SIG_TIMER, timerInterruptHandler);
    signal(SIG_IO, ioInterruptHandler);
    
    printf("[KERNEL] %d processos inicializados\n", NUM_PROCESSES);
    
    // para todos
    for (i = 0; i < NUM_PROCESSES; i++) {
        if (pcb_table[i].state == RUNNING || pcb_table[i].state == READY) {
            kill(pcb_table[i].pid, SIGSTOP);
        }
    }
    sleep(1);
    
    printf("[KERNEL] Round Robin iniciado (quantum=%ds)\n\n", TIME_QUANTUM);
    roundRobinScheduling();
    
    // loop principal
    while (running && total_time < max_time) {
        sleep(1);
        quantum_counter++;
        total_time++;
        
        if (timer_interrupt) {
            timer_interrupt = 0;
            quantum_counter = TIME_QUANTUM;
        }
        
        if (io_interrupt) {
            io_interrupt = 0;
            
            if (read(pipe_input, buffer, sizeof(buffer)) > 0) {
                int proc_id, device;
                char op;
                sscanf(buffer, "IO:%d:%d:%c", &proc_id, &device, &op);
                
                printf("[KERNEL] Syscall I/O: A%d, D%d, op=%c\n", proc_id, device, op);
                
                for (i = 0; i < NUM_PROCESSES; i++) {
                    if (pcb_table[i].id == proc_id) {
                        blockProcess(pcb_table[i].pid, device);
                        
                        if (current_process == i) {
                            current_process = -1;
                            roundRobinScheduling();
                        }
                        
                        sleep(3);
                        unblockProcess(pcb_table[i].pid);
                        break;
                    }
                }
            }
        }
        
        if (quantum_counter >= TIME_QUANTUM) {
            quantum_counter = 0;
            printf("\n[KERNEL] Quantum expirado\n");
            roundRobinScheduling();
        }
        
        // checa se terminou
        int all_done = 1;
        for (i = 0; i < NUM_PROCESSES; i++) {
            if (pcb_table[i].state != FINISHED) {
                all_done = 0;
                break;
            }
        }
        
        if (all_done) {
            printf("\n[KERNEL] Todos finalizaram\n");
            running = 0;
        }
    }
    
    if (total_time >= max_time) {
        printf("\n[KERNEL] Tempo maximo atingido\n");
    }
    
    // mata todos
    for (i = 0; i < NUM_PROCESSES; i++) {
        if (pcb_table[i].state != FINISHED) {
            kill(pcb_table[i].pid, SIGKILL);
        }
    }
    
    printf("[KERNEL] Finalizando\n");
}