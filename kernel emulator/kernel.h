#ifndef KERNEL_H
#define KERNEL_H

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    FINISHED
} ProcessState;

typedef struct {
    pid_t pid;
    int id;
    ProcessState state;
    int blocked_device;
} ProcessControlBlock;

void kernelSim(pid_t process_list[], int pipe_in, int pipe_out);
void timerInterruptHandler(int sig);
void ioInterruptHandler(int sig);
void blockProcess(pid_t pid, int device);
void unblockProcess(pid_t pid);
void roundRobinScheduling();

#endif