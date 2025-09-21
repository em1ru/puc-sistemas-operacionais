#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

time_t inicio = 0;

void inicioHandler(int sig) {
    inicio = time(NULL);
    printf("Ligacao iniciada em %ld\n", inicio);
}

void fimHandler(int sig) {
    if (inicio == 0) {
        printf("Erro: nenhuma ligacao iniciada!\n");
        return;
    }
    time_t fim = time(NULL);
    int duracao = (int)difftime(fim, inicio);
    float preco;

    if (duracao <= 60) {
        preco = duracao * 0.02f;
    } else {
        preco = 60 * 0.02f + (duracao - 60) * 0.01f;
    }

    printf("Ligacao encerrada. Duracao: %d segundos. Custo: R$%.2f\n", duracao, preco);
    inicio = 0;
}

int main(void) {
    signal(SIGUSR1, inicioHandler);
    signal(SIGUSR2, fimHandler);

    printf("Monitor de chamadas iniciado (PID=%d).\n", getpid());
    printf("Use kill -SIGUSR1 <pid> para iniciar e kill -SIGUSR2 <pid> para encerrar.\n");

    while (1) pause();
    return 0;
}
