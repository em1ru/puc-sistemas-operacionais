#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#define EVER ;;

int main (void)
{
    puts("Rodando sem tratadores de sinal...");
    puts("Agora Ctrl-C e Ctrl-\\ vão encerrar direto (comportamento padrão).");

    for(EVER); // loop infinito
}
