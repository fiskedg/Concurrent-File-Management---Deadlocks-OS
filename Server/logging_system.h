#ifndef logging_system_h
#define logging_system_h

#include <stdio.h>
#include <stdlib.h>

extern FILE *loog;

static FILE* createOrOpenLogFile(FILE *loog, char fileName[]);
static void appendLogFile(char message[]);
static void closeLogFile();

#endif // logging_system_h