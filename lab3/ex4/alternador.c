#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h> 

#define EVER ;;

int main(int argc, char* argv[]) {
    pid_t pid_list[2];

    for (int i = 0; i < 2; i++) {
        pid_t pid = fork();

        if (pid == 0) { // Lógica do filho i 
            // Ele vai ser parado pelo pai logo após a criação.
            for(EVER) {
                // O valor de 'i' é copiado para o espaço de memória do filho no momento do fork.
                fprintf(stdout, "filho %d vivo!\n", i);
                fflush(stdout); // Garante que a saída seja impressa imediatamente
                sleep(1); // Um pequeno sleep para não poluir a tela tão rápido
            }
        } 
        else if (pid > 0) {
            // Lógica do pai
            pid_list[i] = pid; // Armazena o PID do filho
            fprintf(stdout, "[pai] filho %d (PID: %d) criado.\n", i, pid);
        } 
        else {
            perror("Fork failed");
            exit(1);
        }
    }

    // Garante que ambos os filhos foram criados antes de começar
    sleep(1); 

    fprintf(stdout, "[PAI] Pausando ambos os filhos para sincronizar...\n");
    kill(pid_list[0], SIGSTOP);
    kill(pid_list[1], SIGSTOP);

    fprintf(stdout, "[PAI] Começando troca de contexto...\n");

    // Vamos iniciar o primeiro filho para dar o pontapé inicial
    fprintf(stdout, "[PAI] Filho 0 acordado, filho 1 parado\n\n");
    kill(pid_list[0], SIGCONT);

    // 10 trocas de contexto
    for (int i = 0; i < 10; i++) {
        sleep(2); // Dando tempo para o filho em execução imprimir algo

        if (i % 2 == 0) {
            // Estava executando o filho 0, agora troca para o filho 1
            fprintf(stdout, "\n[PAI] Parando filho 0, continuando filho 1... (%d)\n\n", i);
            kill(pid_list[0], SIGSTOP);
            kill(pid_list[1], SIGCONT);
        } else {
            // Estava executando o filho 1, agora troca para o filho 0
            fprintf(stdout, "\n[PAI] Parando filho 1, continuando filho 0... (%d)\n\n", i);
            kill(pid_list[1], SIGSTOP);
            kill(pid_list[0], SIGCONT);
        }
    }
    
    sleep(1);

    // Hora do pai matar os filhos
    fprintf(stdout, "\n[PAI] Fim das trocas. Matando os filhos.\n");
    
    if (kill(pid_list[0], SIGKILL) == -1) {
        perror("Error killing child 0");
    } else {
        fprintf(stdout, "[PAI] Filho 0 (PID: %d) morto\tAAAAAAAAAAAAAA (gritos ao fundo).\n", pid_list[0]);
    }
    waitpid(pid_list[0], NULL, 0); // Limpa o processo filho zumbi

    if (kill(pid_list[1], SIGKILL) == -1) {
        perror("Error killing child 1");
    } else {
        fprintf(stdout, "[PAI] Filho 1 (PID: %d) morto.\n", pid_list[1]);
    }
    waitpid(pid_list[1], NULL, 0); // Limpa o processo filho zumbi

    fprintf(stdout, "[PAI] Processo encerrado.\n");

    return 0;
}