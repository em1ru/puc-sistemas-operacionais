#include "genericProcess.h"
#include <signal.h>
#include <sys/types.h>

extern pid_t kernel_pid;

int IOsyscall(int device, char operation) {
    // Avisa ao kernel que foi feita uma syscall de I/O
    
}

int signalHandler(void) {
    ;
}

// Lógica do processo genérico
void genericProcess(void) 
{
    int programCounter;

    for (programCounter = 0; programCounter < MAX_ITER; programCounter++) 
    {
        // Realiza ação genérica 
    }
    return;
}