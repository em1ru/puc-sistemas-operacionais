#ifndef GEN_PROC_H
#define GEN_PROC_H

#define MAX_ITER 1000

int IOsyscall(int device, char operation);

void genericProcess(void);

int signalHandler(void);


#endif