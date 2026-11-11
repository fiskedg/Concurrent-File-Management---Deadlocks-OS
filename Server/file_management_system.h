#ifndef FILE_MANAGEMENT_SYSTEM_H
#define FILE_MANAGEMENT_SYSTEM_H

#include <stdio.h>

FILE *createOrOpenFile(FILE *filePointer, char fileName[]);
void readFile(char fileName[]);
void appendFile(char fileName[], char message[]);
void updateFile(char fileName[], char oldMessage[], char newMessage[]);
void lastModified(char fileName[]);
void listFiles(char directoryName[]);

#endif 