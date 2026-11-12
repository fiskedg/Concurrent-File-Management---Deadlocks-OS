#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>

#define FILE_DIRECTORY "../TextFileFolder/"

static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

 FILE* createOrOpenFile(FILE *filePointer, char fileName[]) {

    pthread_mutex_lock(&file_mutex);

    //Open/Create the file in "w" (write) mode (overrides whatever written)
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s%s", FILE_DIRECTORY, fileName);
    
    filePointer = fopen(fullPath, "w");

    // Always check if the file opened successfully
    if (filePointer == NULL) {
        printf("Error: Could not create or open the file.\n");
        pthread_mutex_unlock(&file_mutex);

        return NULL; // Exit the program with an error code
    }

    printf("%s created successfully.\n", fileName);
    pthread_mutex_unlock(&file_mutex);

    return filePointer;
}

void readFile(char fileName[], int client_fd){
    pthread_mutex_lock(&file_mutex);

    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s%s", FILE_DIRECTORY, fileName);
    FILE *filePointer = fopen(fullPath, "r"); //open a file in read only mode

    if (filePointer == NULL) {
        dprintf(client_fd,"Error opening file.\n");
        pthread_mutex_unlock(&file_mutex);

        return;
    }


char readBuffer[100]; //create a buffer to store the current words in a file being read
while(fgets(readBuffer, sizeof(readBuffer), filePointer) != NULL){ // Reads up to 99 characters or until a newline, storing it in readBuffer
    dprintf(client_fd, "%s", readBuffer);
}
dprintf(client_fd, "\n");
fclose(filePointer);
pthread_mutex_unlock(&file_mutex);

}

void appendFile(char fileName[], char message[], int client_fd){ //Appending adds another entry to the end of the file
    pthread_mutex_lock(&file_mutex);

    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s%s", FILE_DIRECTORY, fileName);
    FILE *filePointer = fopen(fullPath, "a"); //open a file in append mode (doesn't overwrite what has already been written)

// Always check if the file opened successfully
    if (filePointer == NULL) {
        dprintf(client_fd, "Error: Could notopen the file.\n");
        pthread_mutex_unlock(&file_mutex);

        return; // Exit the program with an error code
    }

    //Write text to the file
    if (fputs(message, filePointer) == EOF) {
        dprintf(client_fd, "Error writing to file.\n");
    }

    fclose(filePointer);
    pthread_mutex_unlock(&file_mutex);

}

void updateFile(char fileName[], char oldMessage[], char newMessage[], int client_fd){ //Updating rewrites preexisting entries
    pthread_mutex_lock(&file_mutex);

    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s%s", FILE_DIRECTORY, fileName);
    FILE *originalFilePointer = fopen(fullPath, "r"); //open a file in write mode (overwrites what has already been written)

    if (originalFilePointer == NULL) {
        dprintf(client_fd, "Error opening original file.\n");
        pthread_mutex_unlock(&file_mutex);

        return;
    }
FILE *tempFilePointer = fopen("../TextFileFolder/temp.txt", "w");

    if (tempFilePointer == NULL) {
        dprintf(client_fd, "Error creating temporary file.\n");
        fclose(originalFilePointer);
        pthread_mutex_unlock(&file_mutex);

        return;
    }

char readBuffer[100]; //create a buffer to store the current words in a file being read
int updated = 0; //Create int to act as a boolean for if the file was actually updated

while(fgets(readBuffer, sizeof(readBuffer), originalFilePointer) != NULL){ // Reads up to 99 characters or until a newline, storing it in readBuffer
if (strcmp(readBuffer, oldMessage) == 0) {
            fputs(newMessage, tempFilePointer);
            updated = 1;
        }
        else {
            fputs(readBuffer, tempFilePointer);
        }
}

    fclose(originalFilePointer);
     fclose(tempFilePointer);

    if (updated) {
        if (rename("../TextFileFolder/temp.txt", fullPath) != 0) {
            dprintf(client_fd, "Error replacing original file.\n");
            remove("../TextFileFolder/temp.txt");
            pthread_mutex_unlock(&file_mutex);

            return;
        }

    dprintf(client_fd,"Record successfully updated.\n");
}else{
    dprintf(client_fd, "Record not found. No changes were made.\n");
    remove("../TextFileFolder/temp.txt");
}
pthread_mutex_unlock(&file_mutex);

}


void lastModified(char fileName[], int client_fd){
pthread_mutex_lock(&file_mutex);

struct stat fileInfo;

    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s%s", FILE_DIRECTORY, fileName);
    if (stat(fullPath, &fileInfo) != 0) {
        dprintf(client_fd, "Error retrieving file information.\n");
        pthread_mutex_unlock(&file_mutex);

        return;
    }

   dprintf(client_fd, "File size: %ld bytes\n", (long)fileInfo.st_size);
   dprintf(client_fd, "Last modified: %s", ctime(&fileInfo.st_mtime));
   pthread_mutex_unlock(&file_mutex);

}

void listFiles(char directoryName[], int client_fd){
    pthread_mutex_lock(&file_mutex);

    (void)directoryName;
    DIR *currDirectory = opendir(FILE_DIRECTORY);

// Always check if the directory opened successfully
    if (currDirectory == NULL) {
        dprintf(client_fd, "Error: Could not open the directory.\n");
        pthread_mutex_unlock(&file_mutex);

        return; // Exit the program with an error code
    }

    struct dirent *entry; //Directory entry

    dprintf(client_fd, "Files in directory:\n");
    while((entry =readdir(currDirectory)) != NULL){
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) { //d_name = actual name of the file, entry->d_name allows access to that name starting from the pointer

            dprintf(client_fd, "%s\n", entry->d_name);
        }
    }
    closedir(currDirectory);
    pthread_mutex_unlock(&file_mutex);

}
