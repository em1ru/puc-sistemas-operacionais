#ifndef KERNEL_H
#define KERNEL_H

#include <sys/types.h>

int blockGenericProcess(pid_t pid);

int unblockGenericProcess(pid_t pid);

int interruptionSignalHandler(void);

void roundRobinScheduling(); 

void kernelSim(pid_t procList[]);

#endif