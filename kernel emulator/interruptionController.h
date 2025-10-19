#ifndef INTCONTR_H
#define INTCONTR_H

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void interControllerSim(int pipe_to_kernel);

#endif