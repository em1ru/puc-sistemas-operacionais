#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <wait.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/select.h>

/* ========== CONFIGURAÇÕES DO SIMULADOR ========== */
#define NUM_PROCESSOS 7         // kernel, intercontrol, A1, ..., A5
#define NUM_APPS 5              // A1 até A5
#define TRUE 1
#define FALSE 0
#define MAX_ITERACOES 20        // Número máximo de iterações de cada processo A
#define TIME_SLICE 2            // Tempo do time slice em segundos
#define PROB_IRQ1 10            // Probabilidade de IRQ1 (D1) em % - sugerido 10%
#define PROB_IRQ2 5             // Probabilidade de IRQ2 (D2) em % - sugerido 5%
#define PROB_SYSCALL 15         // Probabilidade de syscall em cada iteração (%)
#define DEBUG 1                 // Ativa logs detalhados (0 = desativa)
/* ========== DEFINIÇÃO DE TIPOS E ESTRUTURAS ========== */

// Estados possíveis de um processo
typedef enum {
    READY,              // Pronto para executar
    RUNNING,            // Executando
    BLOCKED_D1_R,       // Bloqueado esperando leitura em D1
    BLOCKED_D1_W,       // Bloqueado esperando escrita em D1
    BLOCKED_D2_R,       // Bloqueado esperando leitura em D2
    BLOCKED_D2_W,       // Bloqueado esperando escrita em D2
    FINISHED            // Terminou execução
} EstadoProcesso;

// Tipo de mensagem enviada pelos pipes
typedef enum {
    MSG_IRQ,            // Interrupção do InterController
    MSG_SYSCALL         // System call de um processo Ax
} MsgType;

// Operações de syscall possíveis
typedef enum {
    OP_READ,            // Leitura bloqueante
    OP_WRITE,           // Escrita bloqueante
    OP_EXECUTE          // Operação não bloqueante (X)
} SyscallOp;

// Dispositivos disponíveis
typedef enum {
    NO_DEVICE = 0,      // Nenhum dispositivo (para operação X)
    DEV_D1 = 1,         // Dispositivo D1
    DEV_D2 = 2          // Dispositivo D2
} Device;

// Mensagem trafegada pelos pipes
typedef struct {
    MsgType type;               // Tipo: IRQ ou SYSCALL
    int remetente_pid;          // PID do remetente (para syscalls)
    
    // Campos específicos para IRQ
    int irq_type;               // 0 = timeslice, 1 = D1 finished, 2 = D2 finished
    
    // Campos específicos para SYSCALL
    Device device;              // Dispositivo alvo (D1, D2 ou nenhum)
    SyscallOp op;               // Operação (R, W ou X)
} Mensagem;

// Bloco de Controle de Processo (PCB)
typedef struct controleProcesso {
    int pid;                    // PID do processo
    int pc;                     // Program counter (iteração atual)
    EstadoProcesso estado;      // Estado atual do processo
    int d1_count;               // Contador de acessos ao D1
    int d2_count;               // Contador de acessos ao D2
    
    // Contexto da syscall pendente (útil para debug/logs)
    Device pending_device;      // Dispositivo da syscall bloqueante
    SyscallOp pending_op;       // Operação da syscall bloqueante
} ControleProcesso;

/* ========== VARIÁVEIS GLOBAIS (MEMÓRIA COMPARTILHADA) ========== */
/*
 * Índices na Lista de PIDs:
 * 0       -> kernel
 * 1..5    -> A1, A2, A3, A4, A5
 * 6       -> intercontroller
 */
int *lista_pids;            // Array com PIDs de todos os processos
ControleProcesso *lista_cp; // Array com blocos de controle (PCBs)

/* ========== FUNÇÕES AUXILIARES ========== */

// Handler trivial para SIGCONT - apenas permite que pause() seja interrompido
void sigcont_handler(int signum) {
    (void)signum; // Evita warning de parâmetro não usado
}

// Converte enum EstadoProcesso para string legível (útil para logs)
const char* estado_para_string(EstadoProcesso estado) {
    switch (estado) {
        case READY:         return "READY";
        case RUNNING:       return "RUNNING";
        case BLOCKED_D1_R:  return "BLOCKED_D1_R";
        case BLOCKED_D1_W:  return "BLOCKED_D1_W";
        case BLOCKED_D2_R:  return "BLOCKED_D2_R";
        case BLOCKED_D2_W:  return "BLOCKED_D2_W";
        case FINISHED:      return "FINISHED";
        default:            return "UNKNOWN";
    }
}

/*
 * Handler para SIGINT (Ctrl-C)
 * Pausa todos os processos e imprime seus estados atuais.
 * Útil para debugar o simulador em tempo de execução.
 */
void sig_pauseinfo_handler(int signum) {
    (void)signum;
    
    // Primeiro, pausa todos os processos
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        kill(lista_pids[i], SIGSTOP);
    }

    // Agora imprime o estado de cada processo
    fprintf(stderr, "\n========== SIMULADOR PAUSADO ==========\n");
    fprintf(stderr, "Estado atual de todos os processos:\n\n");
    
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        const char* estado_str = estado_para_string(lista_cp[i].estado);
        
        if (i == 0) {
            // Kernel
            fprintf(stderr, "[KERNEL] PID=%d | Estado=%s | PC=%d | D1=%d | D2=%d\n",
                    lista_cp[i].pid, estado_str, lista_cp[i].pc,
                    lista_cp[i].d1_count, lista_cp[i].d2_count);
        } 
        else if (i == 6) {
            // InterController
            fprintf(stderr, "[INTERCONTROL] PID=%d | Estado=%s\n",
                    lista_cp[i].pid, estado_str);
        } 
        else {
            // Processos A1..A5
            fprintf(stderr, "[A%d] PID=%d | Estado=%s | PC=%d/%d | D1=%d | D2=%d",
                    i, lista_cp[i].pid, estado_str, lista_cp[i].pc,
                    MAX_ITERACOES, lista_cp[i].d1_count, lista_cp[i].d2_count);
            
            // Se tiver syscall pendente, mostra qual é
            if (lista_cp[i].estado >= BLOCKED_D1_R && lista_cp[i].estado <= BLOCKED_D2_W) {
                fprintf(stderr, " | Pendente: D%d %s",
                        lista_cp[i].pending_device,
                        (lista_cp[i].pending_op == OP_READ) ? "READ" : "WRITE");
            }
            fprintf(stderr, "\n");
        }
    }
    
    fprintf(stderr, "\n[INFO] Pressione Ctrl-Z para retomar a simulação\n");
    fprintf(stderr, "=======================================\n\n");
    
    // Aguarda sinal de retomada
    pause();
}

/*
 * Handler para SIGTSTP (Ctrl-Z)
 * Retoma todos os processos pausados pelo Ctrl-C.
 */
void sig_despauseinfo_handler(int signum) {
    (void)signum;
    
    fprintf(stderr, "\n[INFO] Retomando simulação...\n\n");
    
    // Retoma todos os processos
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        kill(lista_pids[i], SIGCONT);
    }
}

/* ========== FUNÇÕES DE GERENCIAMENTO DE FILAS ========== */

/*
 * Adiciona um processo ao final da fila de bloqueados.
 * fila: array de índices (1..5 para A1..A5), terminado com -1
 * proc_idx: índice do processo a adicionar (1..5)
 */
void enqueue(int *fila, int proc_idx) {
    int i = 0;
    // Procura o primeiro slot vazio (marcado com -1)
    while (fila[i] != -1 && i < NUM_PROCESSOS) {
        i++;
    }
    if (i < NUM_PROCESSOS) {
        fila[i] = proc_idx;
    }
}

/*
 * Remove e retorna o primeiro processo da fila (FIFO).
 * Desloca os demais para a esquerda.
 * Retorna -1 se a fila estiver vazia.
 */
int dequeue(int *fila) {
    if (fila[0] == -1) {
        return -1; // Fila vazia
    }
    
    int proc_idx = fila[0];
    
    // Desloca todos para a esquerda
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        fila[i] = fila[i + 1];
    }
    
    return proc_idx;
}

/*
 * Verifica se a fila está vazia.
 */
int is_queue_empty(int *fila) {
    return (fila[0] == -1);
}

/* ========== FUNÇÕES DO KERNEL ========== */

/*
 * Mapeia PID para índice na lista_cp.
 * Retorna -1 se não encontrar.
 */
int pid_to_index(int pid) {
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        if (lista_cp[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

/*
 * Encontra o próximo processo READY no round-robin.
 * Busca apenas entre A1..A5 (índices 1..5).
 * Retorna -1 se não houver nenhum processo READY.
 */
int find_next_ready(int current_idx) {
    int next_idx = current_idx + 1;
    
    // Procura circularmente entre A1..A5 (índices 1..5)
    for (int i = 0; i < NUM_APPS; i++) {
        // Mapeia para índices 1..5 (A1..A5)
        int idx = ((next_idx - 1) % NUM_APPS) + 1;
        
        if (lista_cp[idx].estado == READY) {
            return idx;
        }
        next_idx++;
    }
    
    return -1; // Nenhum processo READY encontrado
}

/*
 * Verifica se todos os processos A1..A5 terminaram.
 */
int all_apps_finished() {
    for (int i = 1; i <= NUM_APPS; i++) {
        if (lista_cp[i].estado != FINISHED) {
            return FALSE;
        }
    }
    return TRUE;
}

/* ========== FUNÇÃO PRINCIPAL ========== */

int main(void) {
    int pid;

    /* --- Configuração da Memória Compartilhada --- */
    
    // Aloca memória compartilhada para a lista de PIDs
    lista_pids = mmap(NULL, NUM_PROCESSOS * sizeof(int), 
                      PROT_READ | PROT_WRITE, 
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (lista_pids == MAP_FAILED) {
        perror("mmap para lista_pids falhou");
        exit(1);
    }

    // Aloca memória compartilhada para a lista de PCBs
    lista_cp = mmap(NULL, NUM_PROCESSOS * sizeof(ControleProcesso), 
                    PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (lista_cp == MAP_FAILED) {
        perror("mmap para lista_cp falhou");
        munmap(lista_pids, NUM_PROCESSOS * sizeof(int));
        exit(1);
    }

    /* --- Criação dos Pipes --- */
    
    int pipe_irq[2];      // IRQs: InterController -> Kernel
    int pipe_syscall[2];  // Syscalls: A1..A5 -> Kernel

    if (pipe(pipe_irq) == -1 || pipe(pipe_syscall) == -1) {
        perror("Falha ao criar pipes");
        munmap(lista_pids, NUM_PROCESSOS * sizeof(int));
        munmap(lista_cp, NUM_PROCESSOS * sizeof(ControleProcesso));
        exit(1);
    }

    /* --- Configuração dos Sinais --- */
    
    signal(SIGCONT, sigcont_handler);            // Para retomar processos pausados
    signal(SIGINT, sig_pauseinfo_handler);       // Ctrl-C: mostra logs
    signal(SIGTSTP, sig_despauseinfo_handler);   // Ctrl-Z: retoma após Ctrl-C
    
    /* --- Banner Inicial --- */
    
    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "       KernelSim - Simulador de OS     \n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "Configurações:\n");
    fprintf(stderr, "  - Processos de aplicação: %d (A1..A5)\n", NUM_APPS);
    fprintf(stderr, "  - Iterações máximas: %d\n", MAX_ITERACOES);
    fprintf(stderr, "  - Time slice: %d segundos\n", TIME_SLICE);
    fprintf(stderr, "  - Prob. IRQ1 (D1): %d%%\n", PROB_IRQ1);
    fprintf(stderr, "  - Prob. IRQ2 (D2): %d%%\n", PROB_IRQ2);
    fprintf(stderr, "  - Prob. Syscall: %d%%\n", PROB_SYSCALL);
    fprintf(stderr, "\nControles:\n");
    fprintf(stderr, "  - Ctrl-C: Pausar e mostrar logs\n");
    fprintf(stderr, "  - Ctrl-Z: Retomar após pausa\n");
    fprintf(stderr, "========================================\n\n");
    sleep(1);
    
    fprintf(stderr, "[SIMULADOR] Criando processos...\n");

    /* --- Criação dos Processos (Fork) --- */
    
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        if ((pid = fork()) == -1) {
            fprintf(stderr, "[ERRO] Fork falhou ao criar processo %d\n", i);
            exit(1);
        } 
        else if (pid == 0) {
            /* ========== CÓDIGO DOS PROCESSOS FILHOS ========== */
            
            // Filhos ignoram SIGINT para não morrerem com Ctrl+C
            signal(SIGINT, SIG_IGN);
            
            // Inicializa gerador aleatório com seed única para cada processo
            srand(time(NULL) ^ getpid());
            
            if (DEBUG) {
                const char *nome[] = {"KERNEL", "A1", "A2", "A3", "A4", "A5", "INTERCONTROL"};
                fprintf(stderr, "[%s] Processo criado (PID=%d). Aguardando start...\n", 
                        nome[i], getpid());
            }
            
            // Todos os filhos pausam e esperam o simulador iniciar
            pause();
            
            /* --- Processo Kernel --- */
            if (i == 0) {
                fprintf(stderr, "[KERNEL] Iniciando...\n");

                // Fecha extremidades não usadas dos pipes
                close(pipe_irq[1]);      // Kernel não escreve IRQs
                close(pipe_syscall[1]);  // Kernel não escreve syscalls

                // Filas de processos bloqueados por dispositivo
                // Índices 0..NUM_PROCESSOS armazenam índices de processos (1..5)
                // Valor -1 marca posição vazia
                int fila_d1[NUM_PROCESSOS + 1];
                int fila_d2[NUM_PROCESSOS + 1];
                
                for (int j = 0; j <= NUM_PROCESSOS; j++) {
                    fila_d1[j] = -1;
                    fila_d2[j] = -1;
                }

                // Inicializa os blocos de controle de todos os processos
                for (int j = 0; j < NUM_PROCESSOS; j++) {
                    lista_cp[j].pid = lista_pids[j];
                    lista_cp[j].pc = 0;
                    lista_cp[j].d1_count = 0;
                    lista_cp[j].d2_count = 0;
                    lista_cp[j].pending_device = NO_DEVICE;
                    lista_cp[j].pending_op = OP_EXECUTE;
                    
                    // Estado inicial:
                    // - Kernel e InterController: sempre RUNNING
                    // - A1: começa RUNNING
                    // - A2..A5: começam READY (mas pausados com SIGSTOP)
                    if (j == 0 || j == 6) {
                        lista_cp[j].estado = RUNNING;  // Kernel e InterController
                    } else if (j == 1) {
                        lista_cp[j].estado = RUNNING;  // A1 começa rodando
                    } else {
                        lista_cp[j].estado = READY;    // A2..A5 começam prontos
                    }
                }
                
                // Pausa inicialmente A2..A5 (serão ativados pelo escalonador)
                for (int j = 2; j <= NUM_APPS; j++) {
                    kill(lista_cp[j].pid, SIGSTOP);
                }
                
                fprintf(stderr, "[KERNEL] Estado inicial configurado. A1 executando.\n");
                
                // Despausa A1 para começar
                kill(lista_cp[1].pid, SIGCONT);
                
                // Índice do processo atualmente em execução (começa com A1)
                int current_idx = 1;
                
                // Monitora os dois pipes sem bloquear
                fd_set read_fds;
                int max_fd = (pipe_irq[0] > pipe_syscall[0]) ? pipe_irq[0] : pipe_syscall[0];
                
                Mensagem msg;
                
                /* --- Loop Principal do Kernel --- */
                
                while (!all_apps_finished()) {
                    // Prepara os file descriptors
                    FD_ZERO(&read_fds);
                    FD_SET(pipe_irq[0], &read_fds);
                    FD_SET(pipe_syscall[0], &read_fds);
                    
                    // Timeout de 1 segundo
                    struct timeval timeout;
                    timeout.tv_sec = 1;
                    timeout.tv_usec = 0;
                    
                    int ready = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
                    
                    if (ready == -1) {
                        continue;  // Erro no select, tenta novamente
                    }
                    
                    if (ready == 0) continue;  // Timeout
                    
                    // Processa IRQ se disponível
                    if (FD_ISSET(pipe_irq[0], &read_fds)) {
                        ssize_t bytes_read = read(pipe_irq[0], &msg, sizeof(Mensagem));
                        
                        if (bytes_read == sizeof(Mensagem) && msg.type == MSG_IRQ) {
                            
                            if (msg.irq_type == 0) {
                                // IRQ0: Time Slice
                                if (DEBUG) {
                                    fprintf(stderr, "[KERNEL] IRQ0 recebido (timeslice)\n");
                                }
                                
                                // Pausa processo atual
                                if (lista_cp[current_idx].estado == RUNNING) {
                                    kill(lista_cp[current_idx].pid, SIGSTOP);
                                    lista_cp[current_idx].estado = READY;
                                    
                                    if (DEBUG) {
                                        fprintf(stderr, "[KERNEL] A%d pausado\n", current_idx);
                                    }
                                }
                                
                                // Busca próximo processo
                                int next_idx = find_next_ready(current_idx);
                                
                                if (next_idx != -1 && next_idx != current_idx) {
                                    fprintf(stderr, "[KERNEL] Escalonamento: A%d -> A%d\n", 
                                            current_idx, next_idx);
                                    
                                    current_idx = next_idx;
                                    lista_cp[current_idx].estado = RUNNING;
                                    kill(lista_cp[current_idx].pid, SIGCONT);
                                } else if (next_idx == -1 && DEBUG) {
                                    fprintf(stderr, "[KERNEL] Nenhum processo READY. CPU idle.\n");
                                }
                            }
                            else if (msg.irq_type == 1) {
                                // IRQ1: D1 completou
                                
                                fprintf(stderr, "[KERNEL] IRQ1 recebido (D1 finished)\n");
                                
                                int proc_idx = dequeue(fila_d1);
                                
                                if (proc_idx != -1) {
                                    lista_cp[proc_idx].estado = READY;
                                    fprintf(stderr, "[KERNEL] A%d desbloqueado (D1)\n", proc_idx);
                                    
                                    if (lista_cp[current_idx].estado != RUNNING) {
                                        current_idx = proc_idx;
                                        lista_cp[current_idx].estado = RUNNING;
                                        kill(lista_cp[current_idx].pid, SIGCONT);
                                        fprintf(stderr, "[KERNEL] A%d escalonado imediatamente\n", proc_idx);
                                    }
                                }
                            }
                            else if (msg.irq_type == 2) {
                                // IRQ2: D2 completou
                                
                                fprintf(stderr, "[KERNEL] IRQ2 recebido (D2 finished)\n");
                                
                                int proc_idx = dequeue(fila_d2);
                                
                                if (proc_idx != -1) {
                                    lista_cp[proc_idx].estado = READY;
                                    fprintf(stderr, "[KERNEL] A%d desbloqueado (D2)\n", proc_idx);
                                    
                                    if (lista_cp[current_idx].estado != RUNNING) {
                                        current_idx = proc_idx;
                                        lista_cp[current_idx].estado = RUNNING;
                                        kill(lista_cp[current_idx].pid, SIGCONT);
                                        fprintf(stderr, "[KERNEL] A%d escalonado imediatamente\n", proc_idx);
                                    }
                                }
                            }
                        }
                    }
                    
                    // Processa Syscall se disponível
                    if (FD_ISSET(pipe_syscall[0], &read_fds)) {
                        ssize_t bytes_read = read(pipe_syscall[0], &msg, sizeof(Mensagem));
                        
                        if (bytes_read == sizeof(Mensagem) && msg.type == MSG_SYSCALL) {
                            
                            int proc_idx = pid_to_index(msg.remetente_pid);
                            
                            if (proc_idx < 1 || proc_idx > NUM_APPS) {
                                fprintf(stderr, "[KERNEL] ERRO: Syscall de PID inválido %d\n", 
                                        msg.remetente_pid);
                                continue;
                            }
                            
                            const char *op_str = (msg.op == OP_READ) ? "READ" : 
                                               (msg.op == OP_WRITE) ? "WRITE" : "EXECUTE";
                            
                            fprintf(stderr, "[KERNEL] Syscall de A%d: %s D%d\n", 
                                    proc_idx, op_str, msg.device);
                            
                            lista_cp[proc_idx].pending_device = msg.device;
                            lista_cp[proc_idx].pending_op = msg.op;
                            
                            if (msg.op == OP_EXECUTE) {
                                // Operação X - não bloqueia
                                if (DEBUG) {
                                    fprintf(stderr, "[KERNEL] A%d: operação X (não bloqueia)\n", proc_idx);
                                }
                            }
                            else {
                                // Operações R/W - bloqueiam
                                if (msg.device == DEV_D1) {
                                    lista_cp[proc_idx].d1_count++;
                                    
                                    if (msg.op == OP_READ) {
                                        lista_cp[proc_idx].estado = BLOCKED_D1_R;
                                    } else {
                                        lista_cp[proc_idx].estado = BLOCKED_D1_W;
                                    }
                                    
                                    enqueue(fila_d1, proc_idx);
                                    
                                    if (DEBUG) {
                                        fprintf(stderr, "[KERNEL] A%d bloqueado em D1\n", proc_idx);
                                    }
                                }
                                else if (msg.device == DEV_D2) {
                                    lista_cp[proc_idx].d2_count++;
                                    
                                    if (msg.op == OP_READ) {
                                        lista_cp[proc_idx].estado = BLOCKED_D2_R;
                                    } else {
                                        lista_cp[proc_idx].estado = BLOCKED_D2_W;
                                    }
                                    
                                    enqueue(fila_d2, proc_idx);
                                    
                                    if (DEBUG) {
                                        fprintf(stderr, "[KERNEL] A%d bloqueado em D2\n", proc_idx);
                                    }
                                }
                                
                                kill(lista_cp[proc_idx].pid, SIGSTOP);
                                
                                if (proc_idx == current_idx) {
                                    int next_idx = find_next_ready(current_idx);
                                    
                                    if (next_idx != -1) {
                                        fprintf(stderr, "[KERNEL] Trocando contexto: A%d (bloqueado) -> A%d\n",
                                                current_idx, next_idx);
                                        current_idx = next_idx;
                                        lista_cp[current_idx].estado = RUNNING;
                                        kill(lista_cp[current_idx].pid, SIGCONT);
                                    } else {
                                        if (DEBUG) {
                                            fprintf(stderr, "[KERNEL] Nenhum processo READY após bloqueio\n");
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                /* --- Fim da Simulação --- */
                
                fprintf(stderr, "\n[KERNEL] Todos os processos de aplicação finalizaram!\n");
                fprintf(stderr, "[KERNEL] Resumo final:\n");
                
                for (int j = 1; j <= NUM_APPS; j++) {
                    fprintf(stderr, "  A%d: PC=%d, D1=%d acessos, D2=%d acessos\n",
                            j, lista_cp[j].pc, lista_cp[j].d1_count, lista_cp[j].d2_count);
                }
                
                fprintf(stderr, "[KERNEL] Encerrando InterController...\n");
                kill(lista_pids[6], SIGTERM);
                sleep(1);
                kill(lista_pids[6], SIGKILL);  // Força se não encerrou
                
                fprintf(stderr, "[KERNEL] Encerrando.\n");
                exit(0);
            }
            /* --- Processo InterController --- */
            else if (i == 6) {
                fprintf(stderr, "[INTERCONTROL] Iniciando...\n");
                
                // Fecha extremidades não usadas dos pipes
                close(pipe_irq[0]);       // Não lê IRQs
                close(pipe_syscall[0]);   // Não lê syscalls
                close(pipe_syscall[1]);   // Não escreve syscalls
                
                Mensagem msg;
                msg.type = MSG_IRQ;
                
                /* --- Loop Principal do InterController --- */
                
                while (1) {
                    // Aguarda o time slice
                    sleep(TIME_SLICE);
                    
                    // Envia IRQ0 (timeslice) - sempre enviado
                    msg.irq_type = 0;
                    if (write(pipe_irq[1], &msg, sizeof(Mensagem)) == -1) {
                        perror("[INTERCONTROL] Erro ao enviar IRQ0");
                    } else if (DEBUG) {
                        fprintf(stderr, "[INTERCONTROL] IRQ0 enviado (timeslice)\n");
                    }
                    
                    // Testa IRQ1 (D1 finished)
                    int rand_d1 = rand() % 100;
                    if (rand_d1 < PROB_IRQ1) {
                        msg.irq_type = 1;
                        if (write(pipe_irq[1], &msg, sizeof(Mensagem)) == -1) {
                            perror("[INTERCONTROL] Erro ao enviar IRQ1");
                        } else {
                            fprintf(stderr, "[INTERCONTROL] IRQ1 enviado (D1 finished)\n");
                        }
                    }
                    
                    // Testa IRQ2 (D2 finished)
                    int rand_d2 = rand() % 100;
                    if (rand_d2 < PROB_IRQ2) {
                        msg.irq_type = 2;
                        if (write(pipe_irq[1], &msg, sizeof(Mensagem)) == -1) {
                            perror("[INTERCONTROL] Erro ao enviar IRQ2");
                        } else {
                            fprintf(stderr, "[INTERCONTROL] IRQ2 enviado (D2 finished)\n");
                        }
                    }
                }
                
                fprintf(stderr, "[INTERCONTROL] Encerrando.\n");
                exit(0);
            }
            /* --- Processos de Aplicação A1..A5 --- */
            else {
                fprintf(stderr, "[A%d] Iniciando...\n", i);
                
                // Fecha extremidades não usadas dos pipes
                close(pipe_irq[0]);       // Não lê IRQs
                close(pipe_irq[1]);       // Não escreve IRQs
                close(pipe_syscall[0]);   // Não lê syscalls
                
                /* --- Loop Principal dos Processos Ax --- */
                
                while (lista_cp[i].pc < MAX_ITERACOES) {
                    
                    sleep(1);
                    
                    lista_cp[i].pc++;                    if (DEBUG && lista_cp[i].pc % 5 == 0) {
                        fprintf(stderr, "[A%d] Iteração %d/%d\n", i, lista_cp[i].pc, MAX_ITERACOES);
                    }
                    
                    int rand_syscall = rand() % 100;
                    
                    if (rand_syscall < PROB_SYSCALL) {
                        Mensagem msg;
                        msg.type = MSG_SYSCALL;
                        msg.remetente_pid = getpid();
                        
                        int op_choice = rand() % 3;
                        
                        if (op_choice == 0) {
                            msg.op = OP_EXECUTE;
                            msg.device = NO_DEVICE;
                            
                            if (DEBUG) {
                                fprintf(stderr, "[A%d] Syscall: EXECUTE (não bloqueia)\n", i);
                            }
                        }
                        else {
                            msg.device = (rand() % 2 == 0) ? DEV_D1 : DEV_D2;
                            
                            if (op_choice == 1) {
                                msg.op = OP_READ;
                                fprintf(stderr, "[A%d] Syscall: READ D%d\n", i, msg.device);
                            } else {
                                msg.op = OP_WRITE;
                                fprintf(stderr, "[A%d] Syscall: WRITE D%d\n", i, msg.device);
                            }
                        }
                        
                        if (write(pipe_syscall[1], &msg, sizeof(Mensagem)) == -1) {
                            perror("[A%d] Erro ao enviar syscall");
                        }
                        
                        if (msg.op != OP_EXECUTE) {
                            usleep(100000);
                            
                            if (lista_cp[i].estado == RUNNING) {
                                pause();
                            }
                        }
                    }
                }
                
                lista_cp[i].estado = FINISHED;
                
                fprintf(stderr, "[A%d] Finalizou! Total: D1=%d, D2=%d\n", 
                        i, lista_cp[i].d1_count, lista_cp[i].d2_count);
                
                exit(0);
            }
        }
        else {
            /* ========== PROCESSO PAI (SIMULADOR) ========== */
            
            // Guarda o PID do filho recém-criado
            lista_pids[i] = pid;
        }
    }

    /* --- Todos os processos foram criados --- */
    
    fprintf(stderr, "\n[SIMULADOR] Todos os %d processos criados.\n", NUM_PROCESSOS);
    fprintf(stderr, "[SIMULADOR] Iniciando simulação em 2 segundos...\n");
    sleep(2);
    
    // Fecha pipes no processo pai (não usa)
    close(pipe_irq[0]);
    close(pipe_irq[1]);
    close(pipe_syscall[0]);
    close(pipe_syscall[1]);
    
    fprintf(stderr, "\n========== INICIANDO SIMULAÇÃO ==========\n\n");
    
    // Despausa primeiro o Kernel
    fprintf(stderr, "[SIMULADOR] Ativando Kernel...\n");
    kill(lista_pids[0], SIGCONT);
    sleep(1);
    
    // Depois o InterController
    fprintf(stderr, "[SIMULADOR] Ativando InterController...\n");
    kill(lista_pids[6], SIGCONT);
    sleep(1);
    
    fprintf(stderr, "[SIMULADOR] Sistema operacional ativo!\n\n");
    
    /* --- Aguarda término de todos os processos --- */
    
    int status;
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        pid_t terminated = waitpid(lista_pids[i], &status, 0);
        
        if (terminated > 0) {
            if (WIFEXITED(status)) {
                if (DEBUG) {
                    fprintf(stderr, "[SIMULADOR] Processo PID=%d encerrou (exit=%d)\n",
                            terminated, WEXITSTATUS(status));
                }
            } else if (WIFSIGNALED(status)) {
                if (DEBUG) {
                    fprintf(stderr, "[SIMULADOR] Processo PID=%d morreu por sinal %d\n",
                            terminated, WTERMSIG(status));
                }
            }
        }
    }
    
    fprintf(stderr, "\n========================================\n");
    fprintf(stderr, "   SIMULAÇÃO ENCERRADA COM SUCESSO!   \n");
    fprintf(stderr, "========================================\n\n");

    /* --- Libera memória compartilhada --- */
    
    munmap(lista_pids, NUM_PROCESSOS * sizeof(int));
    munmap(lista_cp, NUM_PROCESSOS * sizeof(ControleProcesso));

    return 0;
}