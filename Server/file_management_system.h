#ifndef FILE_MANAGEMENT_SYSTEM_H
#define FILE_MANAGEMENT_SYSTEM_H

#include <stdio.h>

FILE *createOrOpenFile(FILE *filePointer, char fileName[]);
void readFile(char fileName[], int client_fd);
void appendFile(char fileName[], char message[], int client_fd);
void updateFile(char fileName[], char oldMessage[], char newMessage[], int client_fd);
void lastModified(char fileName[], int client_fd);
void listFiles(char directoryName[], int client_fd);

#endif 