#ifndef EMULATOR_H
#define EMULATOR_H

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "kernel.h"
#include "interruptionController.h"
#include "genericProcess.h"

#define EMUL_PROC_NUM 7 // Número total de processos (Kernel, Controlador de Interrupções e 5 Genéricos)
#define GEN_PROC_NUM (EMUL_PROC_NUM - 2) // Número de processos genéricos (A1 a A5)
#define FORK_ERROR 1

#define KERNEL_FINISH 2
#define INTERCONT_FINISH 3
#define GENPROC_FINISH 4

#endif