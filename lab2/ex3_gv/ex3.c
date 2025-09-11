#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_PROCESS 11
#define VEC_SIZE 1100
#define TARGET 999

int v[VEC_SIZE];

int busca_vetor(int offset) {
    for (int i = offset; i < offset + VEC_SIZE/NUM_PROCESS; i++) {
        if (v[i] ==  TARGET) return i; 
    }
    return -1;
}

int main(void) {
    pid_t pid;
    
    for (int i = 0; i < VEC_SIZE; i++) {
        if ((i % 10) == 0) {
            v[i] = TARGET;
        }
        else {
            v[i] = i;
        }
    }
    
    for (int process_num = 1; process_num < NUM_PROCESS; process_num++) {
        pid = fork();
        int segment = VEC_SIZE / NUM_PROCESS;
        int offset = process_num*segment;

        if (pid == -1) {
            // erro
            perror("fork");
        } 
        else if (pid == 0) {
            // child
            printf("filho %d: posicao %d\n", process_num, busca_vetor(offset));
            exit(process_num);
        }
    }
    // parent (sim, redundante porque o pai está fazendo o papel do child zero na busca)
    printf("pai: posicao %d\n", busca_vetor(0));
    return 0;
}