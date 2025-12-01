#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <wait.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/select.h>

/* ========== CONFIGURAÇÕES DE REDE ========== */
#include <netdb.h>       
#include <arpa/inet.h>   
#include <sys/socket.h>  
#include "protocol.h"    

/* ========== ESTRUTURAS E FUNÇÕES PARA FILE/DIR RESPONSE QUEUE ========== */

// Fila circular para armazenar respostas do servidor aguardando IRQ1 (arquivo) e IRQ2 (diretório)
typedef struct {
    SFSMessage buffer[100]; // Buffer para até 100 mensagens de respostas pendentes
    int head;
    int tail;
    int count;
} ResponseQueue;

// Funções auxiliares para fila (pode colocar antes do main)
void init_queue(ResponseQueue *q) { q->head = 0; q->tail = 0; q->count = 0; }
int is_full(ResponseQueue *q) { return q->count >= 20; }
int is_empty(ResponseQueue *q) { return q->count == 0; }

void push_msg(ResponseQueue *q, SFSMessage msg) {
    if (is_full(q)) return; // Descarte silencioso ou log de erro
    q->buffer[q->tail] = msg;
    q->tail = (q->tail + 1) % 20;
    q->count++;
}

SFSMessage pop_msg(ResponseQueue *q) {
    SFSMessage msg; // Retorno vazio se erro
    if (is_empty(q)) return msg; 
    msg = q->buffer[q->head];
    q->head = (q->head + 1) % 20;
    q->count--;
    return msg;
}

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
    // Área para o Kernel depositar a resposta do servidor para o processo ler
    SFSMessage buffer_resposta;
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

                // --- CONFIGURAÇÃO DE REDE DO KERNEL ---
                int sockfd;
                struct sockaddr_in serveraddr;
                struct hostent *server;
                int portno = 3999; // A mesma porta que você usou no sfss
                char *hostname = "localhost";

                sockfd = socket(AF_INET, SOCK_DGRAM, 0);
                if (sockfd < 0) { perror("ERRO abrindo socket"); exit(1); }

                server = gethostbyname(hostname);
                if (server == NULL) { fprintf(stderr,"ERRO, host não encontrado\n"); exit(0); }

                bzero((char *) &serveraddr, sizeof(serveraddr));
                serveraddr.sin_family = AF_INET;
                bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
                serveraddr.sin_port = htons(portno);

                // Inicializa as filas de resposta
                ResponseQueue file_queue; // Para respostas de Arquivo (IRQ1)
                ResponseQueue dir_queue;  // Para respostas de Diretório (IRQ2)
                init_queue(&file_queue);
                init_queue(&dir_queue);

                // Adiciona o socket ao cálculo do max_fd para o select
                if (sockfd > max_fd) max_fd = sockfd;

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
                    FD_SET(sockfd, &read_fds); // Escuta o socket UDP
                    
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
                                if (!is_empty(&file_queue)) {
                                    // 1. Pega a resposta que chegou da rede e estava na fila
                                    SFSMessage resposta = pop_msg(&file_queue);
                                    int proc_idx = resposta.owner_id; // Identifica quem pediu (A1..A5)
                                    
                                    // 2. Copia a resposta para a memória do processo
                                    lista_cp[proc_idx].buffer_resposta = resposta;
                                    
                                    // 3. Desbloqueia o processo (Tira de BLOCKED)
                                    // Nota: Se estava bloqueado esperando arquivo, estava em BLOCKED_D1_R/W
                                    lista_cp[proc_idx].estado = READY;
                                    
                                    if (DEBUG) {
                                        fprintf(stderr, "[KERNEL] IRQ1: Entregando resposta de ARQUIVO para A%d\n", proc_idx);
                                    }

                                    // 4. Preempção: Se o processo acordado tiver prioridade ou for a vez dele
                                    // (Aqui simplificamos: se o atual não está rodando, roda este)
                                    if (lista_cp[current_idx].estado != RUNNING) {
                                        current_idx = proc_idx;
                                        lista_cp[current_idx].estado = RUNNING;
                                        kill(lista_cp[current_idx].pid, SIGCONT);
                                    }
                                } else {
                                    if (DEBUG) fprintf(stderr, "[KERNEL] IRQ1 recebido, mas fila de Arquivos vazia.\n");
                                }
                            }
                            else if (msg.irq_type == 2) {
                                // --- IRQ2: O InterController permitiu processar DIRETÓRIOS ---
                                // (Lógica idêntica à acima, mas usando dir_queue)
                                
                                if (!is_empty(&dir_queue)) {
                                    SFSMessage resposta = pop_msg(&dir_queue);
                                    int proc_idx = resposta.owner_id;
                                    
                                    lista_cp[proc_idx].buffer_resposta = resposta;
                                    lista_cp[proc_idx].estado = READY;
                                    
                                    if (DEBUG) {
                                        fprintf(stderr, "[KERNEL] IRQ2: Entregando resposta de DIRETÓRIO para A%d\n", proc_idx);
                                    }
                                    
                                    if (lista_cp[current_idx].estado != RUNNING) {
                                        current_idx = proc_idx;
                                        lista_cp[current_idx].estado = RUNNING;
                                        kill(lista_cp[current_idx].pid, SIGCONT);
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
                            if (proc_idx < 1 || proc_idx > NUM_APPS) continue;

                            // 1. Lê a requisição completa da memória compartilhada
                            SFSMessage req = lista_cp[proc_idx].buffer_resposta;
                            
                            if (DEBUG) {
                                fprintf(stderr, "[KERNEL] Syscall de A%d: Tipo %d no Path '%s'\n", 
                                        proc_idx, req.type, req.path);
                            }

                            // 2. Envia para o servidor SFSS via UDP
                            sendto(sockfd, &req, sizeof(SFSMessage), 0, 
                                   (struct sockaddr *)&serveraddr, sizeof(serveraddr));

                            // 3. Define o estado de bloqueio baseado no tipo de operação
                            // O PDF associa IRQ1 a Arquivos e IRQ2 a Diretórios
                            if (req.type == REQ_READ || req.type == REQ_WRITE) {
                                // Operação de Arquivo -> Bloqueia em "D1"
                                lista_cp[proc_idx].estado = BLOCKED_D1_R; // Usando _R como genérico para bloqueio
                            } else {
                                // Operação de Diretório (Create, Remove, List) -> Bloqueia em "D2"
                                lista_cp[proc_idx].estado = BLOCKED_D2_R; 
                            }

                            // 4. Pausa o processo (Sinal SIGSTOP)
                            kill(msg.remetente_pid, SIGSTOP);
                            
                            // 5. Escalonamento: O processo atual parou, precisamos eleger outro
                            if (proc_idx == current_idx) {
                                int next_idx = find_next_ready(current_idx);
                                if (next_idx != -1) {
                                    current_idx = next_idx;
                                    lista_cp[current_idx].estado = RUNNING;
                                    kill(lista_cp[current_idx].pid, SIGCONT);
                                } else {
                                    if (DEBUG) fprintf(stderr, "[KERNEL] CPU Idle (todos bloqueados)\n");
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
                    sleep(1); // Simula processamento
                    lista_cp[i].pc++;
                    
                    // Decide se faz syscall (Probabilidade definida no define, ex: 15%)
                    int rand_prob = rand() % 100;
                    
                    if (rand_prob < PROB_SYSCALL) {
                        
                        // Prepara a estrutura na memória compartilhada
                        lista_cp[i].buffer_resposta.owner_id = i; 
                        
                        int d = rand(); // Número aleatório para decidir o tipo
                        
                        if (d % 2 != 0) { 
                            // --- IMPAR: Operação de ARQUIVO (Read/Write) ---
                            // Path aleatório: "meuarquivo_0.txt" até "meuarquivo_2.txt"
                            sprintf(lista_cp[i].buffer_resposta.path, "meuarquivo_%d.txt", rand() % 3);
                            lista_cp[i].buffer_resposta.offset = (rand() % 5) * 16; // 0, 16, 32...

                            if ((d / 2) % 2 == 0) {
                                // Tipo: READ
                                lista_cp[i].buffer_resposta.type = REQ_READ;
                            } else {
                                // Tipo: WRITE (Preenche payload com algo)
                                lista_cp[i].buffer_resposta.type = REQ_WRITE;
                                strcpy((char*)lista_cp[i].buffer_resposta.data, "DADOS_TESTE_XY");
                            }
                        } 
                        else {
                            // --- PAR: Operação de DIRETÓRIO (Add/Rem/List) ---
                            sprintf(lista_cp[i].buffer_resposta.path, "meudir_%d", rand() % 3);
                            
                            int op_dir = rand() % 3;
                            if (op_dir == 0) {
                                lista_cp[i].buffer_resposta.type = REQ_LISTDIR;
                            } else if (op_dir == 1) {
                                lista_cp[i].buffer_resposta.type = REQ_CREATE_DIR;
                                sprintf(lista_cp[i].buffer_resposta.secondary_name, "subdir_%d", rand()%10);
                            } else {
                                lista_cp[i].buffer_resposta.type = REQ_REMOVE;
                                sprintf(lista_cp[i].buffer_resposta.secondary_name, "subdir_%d", rand()%10);
                            }
                        }

                        // Avisa o Kernel
                        Mensagem msg;
                        msg.type = MSG_SYSCALL;
                        msg.remetente_pid = getpid();
                        write(pipe_syscall[1], &msg, sizeof(Mensagem));
                        
                        // Bloqueia esperando a resposta
                        pause();
                        
                        // Acordou! Verifica o que chegou
                        SFSMessage resultado = lista_cp[i].buffer_resposta;
                        if (resultado.status >= 0) {
                            if (DEBUG) fprintf(stderr, "[A%d] Sucesso na op %d! (Status/Bytes: %d)\n", 
                                              i, resultado.type, resultado.status);
                        } else {
                            if (DEBUG) fprintf(stderr, "[A%d] Erro na op %d (Status: %d)\n", 
                                              i, resultado.type, resultado.status);
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