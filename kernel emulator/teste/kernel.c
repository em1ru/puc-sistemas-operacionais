#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <wait.h>
#include <sys/mman.h> 

#define NUM_PROCESSOS 7 // kernel, intercontrol, A1, ..., A5

typedef enum {
    READY,
    RUNNING,
    BLOCKED_D1_R,
    BLOCKED_D1_W,
    BLOCKED_D2_R,
    BLOCKED_D2_W,
    FINISHED
} EstadoProcesso;

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

/* Pense em: */
// int lista_pids[NUM_PROCESSOS]; // guarda o pid de todos os processos 
// ControleProcesso lista_cp[NUM_PROCESSOS]; // preserva os índices da lista de pids

void sigcont_handler(int signum) {
    ; // apenas para despausar os processos com SIGCONT
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
        switch (i)
        {
            // kernel
            case 0:
                fprintf(stderr, "[LOG KERNEL] pid: %d\n", lista_cp[i].pid);
                fprintf(stderr, "[LOG KERNEL] d1_count: %d\n", lista_cp[i].d1_count);
                fprintf(stderr, "[LOG KERNEL] d2_count: %d\n", lista_cp[i].d2_count);
                fprintf(stderr, "[LOG KERNEL] estado: %d\n", lista_cp[i].estado);
                fprintf(stderr, "[LOG KERNEL] pc: %d\n", lista_cp[i].pc);
                break;

            // intercontrol
            case 6:
                fprintf(stderr, "[LOG INTERCONT] pid: %d\n", lista_cp[i].pid);
                fprintf(stderr, "[LOG INTERCONT] d1_count: %d\n", lista_cp[i].d1_count);
                fprintf(stderr, "[LOG INTERCONT] d2_count: %d\n", lista_cp[i].d2_count);
                fprintf(stderr, "[LOG INTERCONT] estado: %d\n", lista_cp[i].estado);
                fprintf(stderr, "[LOG INTERCONT] pc: %d\n", lista_cp[i].pc);
                break;
            
            // processos A1, ..., A5
            default:
                fprintf(stderr, "[LOG A%d] pid: %d\n", i, lista_cp[i].pid);
                fprintf(stderr, "[LOG A%d] d1_count: %d\n", i, lista_cp[i].d1_count);
                fprintf(stderr, "[LOG A%d] d2_count: %d\n", i, lista_cp[i].d2_count);
                fprintf(stderr, "[LOG A%d] estado: %d\n", i, lista_cp[i].estado);
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

    // Aloca memória compartilhada para a lista de Controle de Processos
    lista_cp = mmap(NULL, NUM_PROCESSOS * sizeof(ControleProcesso), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (lista_cp == MAP_FAILED) {
        perror("mmap para lista_cp falhou");
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
            signal(SIGINT, SIG_IGN);
            fprintf(stderr, "[FILHOS DO SIMULADOR] Nasceu! Pausei\n");
            pause();
            switch (i)
            {
                case 0:
                    fprintf(stderr, "[KERNEL] Acordei!\n");
                    // Criando lista de controle de processos
                    // Inicializando (índice zero pro kernel, de 1 a 5 pros Ax, índice 6 pro intercontroller)
                    for (int i = 0; i < NUM_PROCESSOS; i++) {
                        lista_cp[i].pid = lista_pids[i];
                        lista_cp[i].d1_count = 0; // será sempre 0 pra kernel e intercontrol
                        lista_cp[i].d2_count = 0; // será sempre 0 pra kernel e intercontrol
                        lista_cp[i].estado = READY; // virá a ser somente RUNNING/FINISHED pra kernel e intercontrol 
                        lista_cp[i].pc = 0;
                    }
                    
                    lista_cp[0].estado = RUNNING; // Kernel está sempre rodando
                    
                    while (1) {;} // Loop principal do Kernel

                    fprintf(stderr, "[KERNEL] Morri :(\n");
                    exit(2);
                    break;
                
                case 6:
                    fprintf(stderr, "[INTERCONTROL] Acordei!\n");
                    fprintf(stderr, "[INTERCONTROL] Morri :(\n");
                    exit(2);
                    break;
                
                default:
                    fprintf(stderr, "[A%d] Acordei!\n", i);
                    fprintf(stderr, "[A%d] Morri :(\n", i);
                    exit(2);
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
            break;
        
        case 6:
            fprintf(stderr, "[SIMULADOR] Despausando intercontrol...\n");
            break;
        
        default:
            fprintf(stderr, "[SIMULADOR] Despausando A%d...\n", i);
            break;
        }
        kill(lista_pids[i], SIGCONT);
        sleep(1);
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