#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <wait.h>
#include <sys/mman.h>
#include <time.h> 

#define NUM_PROCESSOS 7 // kernel, intercontrol, A1, ..., A5
#define TRUE 1
#define FALSE 0
#define MAX_ITERACOES 20 // número máximo de iterações de cada processo A
#define TIME_SLICE 2 // tempo do time slice em segundos
typedef enum {
    READY,
    RUNNING,
    BLOCKED_D1_R,
    BLOCKED_D1_W,
    BLOCKED_D2_R,
    BLOCKED_D2_W,
    FINISHED
} EstadoProcesso;

// Coloque isso no topo do seu arquivo, junto com as outras structs
typedef enum {
    MSG_IRQ,
    MSG_SYSCALL
} MsgType;

typedef enum {
    OP_READ,
    OP_WRITE,
    OP_EXECUTE // Para a operação 'X' que não bloqueia processo por I/O
} SyscallOp;

typedef enum {
    NO_DEVICE,
    DEV_D1,
    DEV_D2
} Device;

// Mensagem enviada pelos pipes
typedef struct {
    MsgType type;
    int remetente_pid; // PID do processo Ax que fez a syscall

    // Campos para IRQ
    int irq_type; // 0, 1 ou 2

    // Campos para SYSCALL
    Device device; // D1 ou D2
    SyscallOp op;  // R, W ou X (operação X é não bloqueante)
} Mensagem;

typedef struct controleProcesso
{
    int pid;
    int pc; // program counter do processo
    EstadoProcesso estado;
    int d1_count; // quantidade de vezes que acessou D1
    int d2_count; // quantidade de vezes que acessou D2

} ControleProcesso;

/* Índices na Lista de PIDs:
kernel       - 0
A1           - 1
...
A5           - 5
intercontrol - 6
*/

/* --- Variáveis Globais em Memória Compartilhada --- */
// São ponteiros que apontarão para a memória compartilhada
int *lista_pids;
ControleProcesso *lista_cp;

void sigcont_handler(int signum) {
    ; // apenas para despausar os processos com SIGCONT
}

// Converte o enum EstadoProcesso para uma string legível
const char* estado_para_string(EstadoProcesso estado) {
    switch (estado) {
        case READY:
            return "READY";
        case RUNNING:
            return "RUNNING";
        case BLOCKED_D1_R:
            return "BLOCKED em D1 (Read)";
        case BLOCKED_D1_W:
            return "BLOCKED em D1 (Write)";
        case BLOCKED_D2_R:
            return "BLOCKED em D2 (Read)";
        case BLOCKED_D2_W:
            return "BLOCKED em D2 (Write)";
        case FINISHED:
            return "FINISHED";
        default:
            return "UNKNOWN_STATE";
    }
}

// control-C <pid-simulador> deve pausar TODOS os processos e imprimir controleProcessos de cada um
void sig_pauseinfo_handler(int signum) {
    
    // pausa todos os processos primeiro
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        kill(lista_pids[i], SIGSTOP);
    }

    // imprime informações de cada um
    fprintf(stderr, "\n[SIMULADOR PAUSADO] Imprimindo Logs dos Processos:\n");
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        // Pega a string do estado uma vez para evitar chamar a função múltiplas vezes
        const char* estado_str = estado_para_string(lista_cp[i].estado);
        switch (i)
        {
            // kernel
            case 0:
                fprintf(stderr, "[LOG KERNEL] pid: %d\n", lista_cp[i].pid);
                fprintf(stderr, "[LOG KERNEL] d1_count: %d\n", lista_cp[i].d1_count);
                fprintf(stderr, "[LOG KERNEL] d2_count: %d\n", lista_cp[i].d2_count);
                fprintf(stderr, "[LOG KERNEL] estado: %s\n", estado_str); 
                fprintf(stderr, "[LOG KERNEL] pc: %d\n", lista_cp[i].pc);
                break;

            // intercontrol
            case 6:
                fprintf(stderr, "[LOG INTERCONT] pid: %d\n", lista_cp[i].pid);
                fprintf(stderr, "[LOG INTERCONT] d1_count: %d\n", lista_cp[i].d1_count);
                fprintf(stderr, "[LOG INTERCONT] d2_count: %d\n", lista_cp[i].d2_count);
                fprintf(stderr, "[LOG INTERCONT] estado: %s\n", estado_str);  
                fprintf(stderr, "[LOG INTERCONT] pc: %d\n", lista_cp[i].pc);
                break;
            
            // processos A1, ..., A5
            default:
                fprintf(stderr, "[LOG A%d] pid: %d\n", i, lista_cp[i].pid);
                fprintf(stderr, "[LOG A%d] d1_count: %d\n", i, lista_cp[i].d1_count);
                fprintf(stderr, "[LOG A%d] d2_count: %d\n", i, lista_cp[i].d2_count);
                fprintf(stderr, "[LOG A%d] estado: %s\n", i, estado_str);  
                fprintf(stderr, "[LOG A%d] pc: %d\n", i, lista_cp[i].pc);
                break;
        }
    }
    fprintf(stderr, "[SIMULADOR PAUSADO] Esperando Control-Z para me despausar\n");
    pause();
}

// control-Z <pid-simulador> deve despausar TODOS os processos que foram pausados para log com control-C
void sig_despauseinfo_handler(int signum) {
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        kill(lista_pids[i], SIGCONT);
    }
}

int main(void) {
    int pid;

    // --- Configuração da Memória Compartilhada ---
    // Aloca memória compartilhada para a lista de PIDs
    lista_pids = mmap(NULL, NUM_PROCESSOS * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (lista_pids == MAP_FAILED) {
        perror("mmap para lista_pids falhou");
        exit(1);
    }
    
    /* Pense em: */
    // int lista_pids[NUM_PROCESSOS]; // guarda o pid de todos os processos 
    // ControleProcesso lista_cp[NUM_PROCESSOS]; // preserva os índices da lista de pids

    // Aloca memória compartilhada para a lista de Controle de Processos
    lista_cp = mmap(NULL, NUM_PROCESSOS * sizeof(ControleProcesso), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (lista_cp == MAP_FAILED) {
        perror("mmap para lista_cp falhou");
        exit(1);
    }

    int pipe_irq[2]; // pipe para IRQs (do intercontrol pro kernel) 
    int pipe_syscall[2]; // pipe para syscalls (dos processos Ax pro kernel)

    if (pipe(pipe_irq) == -1 || pipe(pipe_syscall) == -1) {
        perror("Falha ao criar pipes");
        exit(1);
    }

    signal(SIGCONT, sigcont_handler); // SIGCONT trivial
    signal(SIGINT, sig_pauseinfo_handler); // control C (kill -2 <PID>)
    signal(SIGTSTP, sig_despauseinfo_handler); // control Z (kill -20 <PID>)
    
    fprintf(stderr, "==== KernelSim ====\n");
    sleep(1);
    fprintf(stderr, ">> Espere pelo processo Kernel começar.\n");
    sleep(1);
    fprintf(stderr, ">> Pressione Control-C para logs.\n");
    fprintf(stderr, ">> Pressione Control-Z para sair dos logs.\n");
    sleep(1);
    // fprintf(stderr, "A simulação começará dentro de 3 segundos...\n");
    // sleep(1);
    // fprintf(stderr, "Ansioso?\n");
    // sleep(1);
    // fprintf(stderr, "Contagem regressiva começa agora:\n");
    // sleep(1);
    // fprintf(stderr, "100...\n");
    // sleep(1);
    // fprintf(stderr, "99...\n");
    // sleep(1);
    // fprintf(stderr, "Brincadeira, começou!\n");    

    for (int i = 0; i < NUM_PROCESSOS; i++) {
        if ((pid = fork()) == -1) { // erro 
            fprintf(stderr, "Erro no fork criando processo de índice %d\n", i);
            exit(1);
        } 
        else if (pid == 0) { // processo filho (processos do simulador)
            // filhos ignoram SIGINT para não morrerem com Ctrl+C !!
            Mensagem msg;
            signal(SIGINT, SIG_IGN);
            fprintf(stderr, "[FILHOS DO SIMULADOR] Nasceu! Pausei\n");
            pause();
            switch (i)
            {
                case 0: // Processo Kernel
                    fprintf(stderr, "[KERNEL] Acordei!\n");

                    close(pipe_irq[1]); // Não escreve IRQs
                    close(pipe_syscall[1]); // Não escreve syscalls

                    // filas de índices: 1, ..., 5 para processos A1, ..., A5
                    int fila_bloqueados_d1[NUM_PROCESSOS + 1]; // fila de bloqueados por D1, último índice é setado para -1 e usado apenas para marcar posição vazia (útil quando deslocar a fila pra esquerda de 1 unidade) 
                    int fila_bloqueados_d2[NUM_PROCESSOS + 1]; // analogamente para D2
                    for (int j = 0; j <= NUM_PROCESSOS; j++) { // inicializa filas de bloqueados
                        fila_bloqueados_d1[j] = -1;
                        fila_bloqueados_d2[j] = -1;
                    }

                    // Criando lista de controle de processos
                    // Inicializando (índice zero pro kernel, de 1 a 5 pros Ax, índice 6 pro intercontroller)
                    for (int i = 0; i < NUM_PROCESSOS; i++) {
                        lista_cp[i].pid = lista_pids[i];
                        lista_cp[i].d1_count = 0; // será sempre 0 pra kernel e intercontrol
                        lista_cp[i].d2_count = 0; // será sempre 0 pra kernel e intercontrol
                        lista_cp[i].estado = ((i <= 1 || i == 6) ? RUNNING : READY ); // somente RUNNING pra kernel, intercontrol e A1 no início (Kernel e Intercontroller estão sempre rodando)
                        lista_cp[i].pc = 0;
                        
                        if (i > 1 && i < 6) kill(lista_cp[i].pid, SIGSTOP); // os processos A2, ..., A5 começam inicialmente parados
                    }
                    
                    ControleProcesso *current_process = &lista_cp[1]; // começa com A1 rodando
                    int indice_modulo = 1; // índice do processo atual no round-robin, módulo NUM_PROCESSOS, começando em A1
                    fprintf(stderr, "[KERNEL] Despausando A1...\n"); // ponta-pé inicial dos Ax com o A1; os demais apenas começarão depois do round-robin liberar
                    fprintf(stderr, "[KERNEL] Despausando A2, A3, A4, A5 somente depois com escalonador do Kernel\n");
                    kill(lista_pids[1], SIGCONT); // despausa A1

                    fd_set read_fds;
                    int pipe_irq_fd = pipe_irq[0];
                    int pipe_syscall_fd = pipe_syscall[0];

                    // Loop principal do Kernel
                    int running = TRUE;
                    while (running) {
                        FD_ZERO(&read_fds); // Limpa o conjunto de monitoramento
                        FD_SET(pipe_irq_fd, &read_fds); // Adiciona o pipe de IRQ ao monitoramento
                        FD_SET(pipe_syscall_fd, &read_fds); // Adiciona o pipe de Syscall ao monitoramento
                        // 1. Espera por uma mensagem (IRQ ou SYSCALL)
                        read(pipe_irq[0], &msg, sizeof(Mensagem));
                        if (msg.type == MSG_IRQ) {
                            if (msg.irq_type == 0) { // IRQ0 - Time Slice
                                fprintf(stderr, "[KERNEL] Recebi IRQ0 (Time Slice) do Intercontrol\n");
                                
                                // Pausa o processo atual
                                kill(current_process->pid, SIGSTOP);
                                current_process->estado = READY;

                                // Busca pelo o próximo processo READY, não bloqueado ou finalizado na lista (round-robin)
                                int next_index = (indice_modulo + 1) % NUM_PROCESSOS;
                                while (lista_cp[next_index].estado != READY) {
                                    next_index = (next_index + 1) % NUM_PROCESSOS;
                                }

                                fprintf(stderr, "[KERNEL] Escalonando: sai A%d, entra A%d\n", indice_modulo, next_index);
                                current_process = &lista_cp[next_index]; // aponta para o próximo processo a ser executado
                                indice_modulo = next_index; // índice atualizado para o próximo processo a ser executado no escalonamento

                                // Despausa o próximo processo
                                kill(current_process->pid, SIGCONT);
                                current_process->estado = RUNNING;
                            }
                            else if (msg.irq_type == 1) { // IRQ1 - D1 finished
                                fprintf(stderr, "[KERNEL] Recebi IRQ1 (D1) do Intercontrol\n");
                                // Desbloqueia o processo mais prioritário que estava bloqueado por D1
                                int prioridade = fila_bloqueados_d1[0];
                                lista_cp[prioridade].estado = READY;
                                fprintf(stderr, "[KERNEL] Desbloqueado A%d em D1 por causa de IRQ1\n", prioridade);
                                for (int j = 0; j < NUM_PROCESSOS; j++) { // desloca a fila de bloqueados D1 para a esquerda
                                    fila_bloqueados_d1[j] = fila_bloqueados_d1[j + 1]; // note que o último índice ser -1 ajuda a rearrumar a fila correta
                                }
                            }
                            else if (msg.irq_type == 2) { // IRQ2 - D2 finished
                                fprintf(stderr, "[KERNEL] Recebi IRQ2 (D2) do Intercontrol\n");
                                // Desbloqueia o processo mais prioritário que estava bloqueado por D2
                                int prioridade = fila_bloqueados_d2[0];
                                lista_cp[prioridade].estado = READY;
                                fprintf(stderr, "[KERNEL] Desbloqueado A%d em D2 por causa de IRQ2\n", prioridade);
                                for (int j = 0; j < NUM_PROCESSOS; j++) { // desloca a fila de bloqueados D1 para a esquerda
                                    fila_bloqueados_d2[j] = fila_bloqueados_d2[j + 1]; // note que o último índice ser -1 ajuda a rearrumar a fila correta
                                }
                            }
                        }   
                        read(pipe_syscall[0], &msg, sizeof(Mensagem));
                        if (msg.type == MSG_SYSCALL) {
                            if (msg.op == OP_READ) {
                                if (msg.device == DEV_D1) {
                                    lista_cp[i].d1_count += 1;
                                    lista_cp[i].estado = BLOCKED_D1_R;
                                }
                                else if (msg.device == DEV_D2) {
                                    lista_cp[i].d2_count += 1;
                                    lista_cp[i].estado = BLOCKED_D2_R;
                                }
                            }
                            else if (msg.op == OP_WRITE) {
                                if (msg.device == DEV_D1) {
                                    lista_cp[i].d1_count += 1;
                                    lista_cp[i].estado = BLOCKED_D1_W;
                                }
                                else if (msg.device == DEV_D2) {
                                    lista_cp[i].d2_count += 1;
                                    lista_cp[i].estado = BLOCKED_D2_W;
                                }
                            }
                            else if (msg.op == OP_EXECUTE) {
                                // operação não bloqueante, apenas incrementa o contador
                                // e o processo continua em RUNNING
                            }
                        
                            kill(lista_cp[i].pid, SIGSTOP); // para o processo que fez a syscall (redundante se já houver pause() pelo processo Ax)
                           
                            running = FALSE; // deve haver processo vivo para continuar rodando
                            for (int i = 1; i < NUM_PROCESSOS; i++) {
                                if (lista_cp[i].estado != FINISHED) {
                                    running = TRUE;
                                    break;
                                }
                            }
                        }
                    }
                    fprintf(stderr, "[KERNEL] Todos os processos finalizaram\n");
                    fprintf(stderr, "[KERNEL] Encerrando Intercontroller\n");
                    kill(lista_pids[6], SIGKILL); // mata o intercontroller
                    fprintf(stderr, "[KERNEL] Morri :(\n");
                    exit(2);
                    break;
                
                case 6: // Processo Intercontrol
                    // fechar extremidades não usadas dos pipes
                    close(pipe_irq[0]); // Não lê do pipe de IRQ
                    close(pipe_syscall[0]); // Não lê syscalls
                    close(pipe_syscall[1]); // Não escreve syscalls

                    fprintf(stderr, "[INTERCONTROL] Acordei!\n");
                    msg.type = MSG_IRQ;
                    int temp;

                    while (1) { // Loop principal do Intercontrol
                        for (int i = 0; i < NUM_PROCESSOS; i++) {
                            while (lista_cp[i].estado == RUNNING) { // apenas se houver alguém executando, o intercontrol deve intervir com o time slice
                                sleep(TIME_SLICE); // O enunciado especifica 500 ms ou 2s como time slices sugeridos
        
                                // 1. Enviar IRQ0 (Timeslice) - sempre
                                msg.irq_type = 0;
                                write(pipe_irq[1], &msg, sizeof(Mensagem));
                                
                                // 2. Tentar enviar IRQ2 (D2 finished) 
                                if ((temp = (rand() % 1000)) < 10) { // Probabilidade de 1%
                                    msg.irq_type = 2;
                                    write(pipe_irq[1], &msg, sizeof(Mensagem));
                                }
        
                                // 3. Tentar enviar IRQ1 (D1 finished) 
                                else if (temp < 200) { // Probabilidade de 20%
                                    msg.irq_type = 1;
                                    write(pipe_irq[1], &msg, sizeof(Mensagem));
                                }
                            }
                            break; // recomeça o for para encontrar o próximo processo rodando
                        }
                    }

                    fprintf(stderr, "[INTERCONTROL] Morri :(\n");
                    exit(2);
                    break;
                
                default: // Processos A1, ..., A5
                    // fechar extremidades não usadas dos pipes de IRQs
                    close(pipe_irq[0]);
                    close(pipe_irq[1]);
                    close(pipe_syscall[0]); // Não lê syscalls, apenas escreve
                    fprintf(stderr, "[A%d] Acordei!\n", i);

                    // Loop principal dos processos Ax
                    while (lista_cp[i].pc < MAX_ITERACOES) {
                        sleep(1); // O PDF sugere sleep(1) no corpo do loop 
                        lista_cp[i].pc++; // Atualiza o PC na memória compartilhada

                        // Probabilidade de 15% de gerar uma syscall
                        if ((rand() % 100) < 15) {
                            Mensagem msg;
                            msg.type = MSG_SYSCALL;
                            msg.remetente_pid = lista_cp[i].pid;
                            
                            // Escolhe dispositivo e operação aleatoriamente 
                            int op_rand = rand() % 3;
                            if (op_rand == 0) {
                                msg.op = OP_EXECUTE;
                                msg.device = NO_DEVICE; // D0 seria NO_DEVICE
                            }
                            else if (op_rand == 1) {
                                msg.op = OP_READ;
                                msg.device = (rand() % 2 == 0) ? DEV_D1 : DEV_D2;
                            }
                            else {
                                msg.op = OP_WRITE;
                                msg.device = (rand() % 2 == 0) ? DEV_D1 : DEV_D2;
                            }

                            fprintf(stderr, "[A%d] Fazendo syscall para D%d\n", i, msg.device);
                            // Envia a syscall para o Kernel
                            write(pipe_syscall[1], &msg, sizeof(Mensagem));
                            
                            // Depois de pedir, o processo fica parado esperando o Kernel pará-lo.
                            // O pause() aqui garante que ele não continue executando até o SIGSTOP chegar.
                            pause(); // TODO: verificar se pode remover esse pause() e apenas o kernel parar com stop 
                        }
                    }
                    
                    lista_cp[i].estado = FINISHED;
                    fprintf(stderr, "[A%d] Terminei! Morri :(\n", i);
                    exit(0);
                    break;
            }

        }
        else { // processo pai (simulador)
            lista_pids[i] = pid;
        }
    }
    sleep(1);
    fprintf(stderr, "Iniciando processos simultaneamente:\n");
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        switch (i)
        {
        case 0:
            fprintf(stderr, "[SIMULADOR] Despausando kernel...\n");
            kill(lista_pids[i], SIGCONT);
            break;

        case 6:
            fprintf(stderr, "[SIMULADOR] Despausando intercontrol...\n");
            kill(lista_pids[i], SIGCONT);
            break;
        }
        // sleep(1);
    }

    // Espera pelo término de todos os filhos
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        waitpid(lista_pids[i], NULL, 0);
    }
    fprintf(stderr, "[SIMULADOR] Geral morreu, finalizando simulador...\n");

    // Libera a memória compartilhada
    munmap(lista_pids, NUM_PROCESSOS * sizeof(int));
    munmap(lista_cp, NUM_PROCESSOS * sizeof(ControleProcesso));

    return 0;
}