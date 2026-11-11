#ifndef FILE_MANAGEMENT_SYSTEM_H
#define FILE_MANAGEMENT_SYSTEM_H

#include <stdio.h>
#define FILE_DIRECTORY "../TextFileFolder/"

FILE *createOrOpenFile(FILE *filePointer, char fileName[]);
int readFile(char fileName[], int client_fd);
int appendFile(char fileName[], char message[]);
int updateFile(char fileName[], char oldMessage[], char newMessage[]);
int lastModified(char fileName[], int client_fd);
int listFiles(char directoryName[], int client_fd);

#endif 