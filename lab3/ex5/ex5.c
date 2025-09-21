#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void trataFpe(int sig) {
    printf("Erro: divisao por zero (sinal %d)\n", sig);
    exit(1);
}

int main(void) {
    int a, b;
    signal(SIGFPE, trataFpe);

    printf("Digite dois numeros: ");
    scanf("%d %d", &a, &b);

    printf("Soma: %d\n", a + b);
    printf("Subtracao: %d\n", a - b);
    printf("Multiplicacao: %d\n", a * b);
    printf("Divisao: %d\n", a / b);  // aqui pode gerar SIGFPE

    return 0;
}
