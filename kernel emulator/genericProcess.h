#ifndef GEN_PROC_H
#define GEN_PROC_H

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void genericProcess(int process_id, int pipe_to_kernel, int pipe_from_kernel);

#endif