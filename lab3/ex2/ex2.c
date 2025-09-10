#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#define EVER ;;

void handler(int sinal)
{
    printf("Sinal %d recebido!\n", sinal);
}

int main(void)
{
    if (signal(SIGKILL, handler) == SIG_ERR) { //pra capturar SIGKILL
        printf("Não foi possível instalar tratador para SIGKILL.\n");
    }

    puts("Processo rodando... tente matar com kill -9 <pid>");
    for(EVER);

    return 0;
}
