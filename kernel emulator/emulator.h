#ifndef EMULATOR_H
#define EMULATOR_H

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

#define NUM_PROCESSES 5
#define TIME_QUANTUM 2
#define MAX_ITERATIONS 10

#define FORK_ERROR 1
#define KERNEL_FINISH 2
#define INTERCONT_FINISH 3
#define GENPROC_FINISH 4

#define DEVICE_D1 1
#define DEVICE_D2 2

#define SIG_TIMER SIGUSR1
#define SIG_IO SIGUSR2

#endif