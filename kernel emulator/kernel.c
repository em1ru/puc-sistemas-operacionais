#include "kernel.h"
#include "emulator.h"

static ProcessControlBlock pcb_table[NUM_PROCESSES];
static int current_process = -1; // índice do processo em execução no momento
static int ready_queue[NUM_PROCESSES]; // fila dos processos em estado READY (prontos para serem executados)
static int ready_count = 0; // próximo índice vago no vetor ready_queue 
static int pipe_input; // "pipe_to_kernel": pipe que processos genéricos escrevem seus pedidos de I/O
// static int intercontrol_input_pipe; // "intercontrol_kernel_pipe": pipe em que intercontroller interrompe I/O nos devices
static int* pc_input_pipes; // ponteiro para o array de pipes de PC
static sig_atomic_t timer_interrupt = 0; // boleano que identifica que estourou time slice do round robin
static sig_atomic_t io_interrupt = 0; // booleano que identifica se houve I/O
static int running = 1; // booleano que vai pra zero somente após término de todos os processos genéricos 
static pid_t perfoming_device1[NUM_PROCESSES]; // lista de índices dos processos realizando I/O no device 1, que serão liberados (por ordem de chegada) devido ao intercontroller 
static pid_t perfoming_device2[NUM_PROCESSES]; // lista de índices dos processos realizando I/O no device 2, que serão liberados (por ordem de chegada) devido ao intercontroller 

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
    int pc_value;

    // para processo atual
    if (current_process >= 0 && pcb_table[current_process].state == RUNNING) {

        // lê o valor do PC do pipe antes de parar o processo
        // a leitura é não-bloqueante para o caso de o pipe estar vazio
        if (read(pc_input_pipes[pcb_table[current_process].id - 1], &pc_value, sizeof(pc_value)) > 0) {
            pcb_table[current_process].pc = pc_value;
        }

        kill(pcb_table[current_process].pid, SIGSTOP);
        pcb_table[current_process].state = READY;
        ready_queue[ready_count++] = current_process;
        printf("[KERNEL] A%d preemptado (PC=%d, D1=%d, D2=%d)\n", pcb_table[current_process].id, pcb_table[current_process].pc, pcb_table[current_process].d1_count, pcb_table[current_process].d2_count);    }
    
    // pega proximo da fila
    if (ready_count > 0) {
        current_process = ready_queue[0];
        
        // remove da fila
        for (i = 0; i < ready_count - 1; i++) {
            ready_queue[i] = ready_queue[i + 1];
        }
        ready_count--;
        
        pcb_table[current_process].state = RUNNING;
        printf("[KERNEL] Escalonando A%d (PC=%d)\n", pcb_table[current_process].id, pcb_table[current_process].pc);
        kill(pcb_table[current_process].pid, SIGCONT);
    } else {
        current_process = -1;
        printf("[KERNEL] Nenhum processo pronto (IDLE)\n");
    }
}

void kernelSim(pid_t process_list[], int pipe_in, int intercontrol_input_pipe, int pc_pipes[]) {
    int i;
    char buffer[256];
    int total_time = 0;
    int max_time = MAX_ITERATIONS * TIME_QUANTUM * NUM_PROCESSES;
    
    printf("[KERNEL] Iniciado (PID %d)\n", getpid());
    
    pipe_input = pipe_in;
    pc_input_pipes = pc_pipes; 

    // Configura pipes de PC para leitura não-bloqueante
    for (i = 0; i < NUM_PROCESSES; i++) {
        fcntl(pc_input_pipes[i], F_SETFL, O_NONBLOCK);
    }
    
    // inicializa PCBs
    for (i = 0; i < NUM_PROCESSES; i++) {
        pcb_table[i].pid = process_list[i];
        pcb_table[i].id = i + 1;
        pcb_table[i].state = READY;
        pcb_table[i].blocked_device = 0;
        pcb_table[i].pc = 0; // Inicializa PC
        pcb_table[i].d1_count = 0; // Inicializa contagem D1
        pcb_table[i].d2_count = 0; // Inicializa contagem D2
        ready_queue[i] = i;
        ready_count++;
    }

    // inicializa lista de pids de processos usando devices
    for (int i = 0; i < NUM_PROCESSES; i++) {
        perfoming_device1[i] = -1;
        perfoming_device2[i] = -1;
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
        total_time++;
        
        // intercontroller envia sinal de timer, o handler do kernel muda a variável pra 1, 
        // então libera esse if que troca o contexto e passa ao próximo processo em execução
        if (timer_interrupt) {
            timer_interrupt = 0;
            printf("\n[KERNEL] Quantum expirado\n");
            roundRobinScheduling();
        }
        
        if (io_interrupt) { // se algum processo sinalizar que começou operação de I/O
            io_interrupt = 0;
            
            // buscando qual processo chamou e qual operação foi chamada
            if (read(pipe_input, buffer, sizeof(buffer)) > 0) {
                int proc_id, device, d1, d2;
                char op;
                sscanf(buffer, "IO:%d:%d:%c:%d:%d", &proc_id, &device, &op, &d1, &d2);

                printf("[KERNEL] Syscall I/O: A%d, D%d, op=%c\n", proc_id, device, op); 
                
                // colocar esse processo em estado de BLOQUEIO até que a operação de I/O seja concluída
                for (i = 0; i < NUM_PROCESSES; i++) {
                    if (pcb_table[i].id == proc_id) { // busca na tabela de informações dos processos a sua referência
                        
                        // atualiza contagem de acessos no PCB
                        pcb_table[i].d1_count = d1;
                        pcb_table[i].d2_count = d2;

                        // adicionar pid do processo à fila de processos executando I/O 
                        if (device == 1) {
                            for (int x = 0; x < NUM_PROCESSES; x++) {
                                if (perfoming_device1[x] == -1) {
                                    perfoming_device1[x] = pcb_table[i].pid;
                                    break;
                                }
                            }
                        }
                        else {
                            for (int x = 0; x < NUM_PROCESSES; x++) {
                                if (perfoming_device2[x] == -1) {
                                    perfoming_device2[x] = pcb_table[i].pid;
                                    break;
                                }
                            }
                        }

                        // bloqueia processo
                        blockProcess(pcb_table[i].pid, device); 
                        
                        if (current_process == i) {
                            current_process = -1;
                            roundRobinScheduling();
                        }
                        break;
                    }
                }
            }
        }

        // tratando os envios de IRQ1 e IRQ2 do intercontroller
        if (read(intercontrol_input_pipe, buffer, sizeof(buffer)) > 0) {
            int device;
            sscanf(buffer, "IRQ%d", &device);

            // percorrer lista de pids fazendo I/O no respectivo device para liberar o mais prioritário (ordem de chegada)
            if (device == 1) {
                for (int j = 0; j < NUM_PROCESSES; j++) {
                    if (perfoming_device1[j] != -1) {
                        // ao encontrar o dispositivo mais prioritário, liberá-lo
                        unblockProcess(perfoming_device1[j]); // passa o PID do processo

                        // puxa todo mundo da lista 1 unidade pra esquerda
                        for (int k = 0; k < NUM_PROCESSES - 1; k++) {
                            perfoming_device1[k] = perfoming_device1[k + 1];
                        }
                        perfoming_device1[NUM_PROCESSES - 1] = -1; // última posição fica livre, não há quem copiar mais à direita
                    }
                }
            } 
            // agora análogo pro caso device == 2
            else { 
                for (int j = 0; j < NUM_PROCESSES; j++) {
                    if (perfoming_device2[j] != -1) {
                        // ao encontrar o dispositivo mais prioritário, liberá-lo
                        unblockProcess(perfoming_device2[j]); // passa o PID do processo
                        
                        // puxa todo mundo da lista 1 unidade pra esquerda
                        for (int k = 0; k < NUM_PROCESSES - 1; k++) {
                            perfoming_device2[k] = perfoming_device2[k + 1];
                        }
                        perfoming_device2[NUM_PROCESSES - 1] = -1; // última posição fica livre, não há quem copiar mais à direita
                    }
                }
            } 
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
    
    // mata todos que ainda não terminaram de executar
    for (i = 0; i < NUM_PROCESSES; i++) {
        if (pcb_table[i].state != FINISHED) {
            kill(pcb_table[i].pid, SIGKILL);
        }
    }
    
    printf("[KERNEL] Finalizando\n");
}