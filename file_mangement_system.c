#include <stdio.h>
#include <stdlib.h>

static void createOrOpenFile(FILE *filePointer, char fileName[]) {

    //Open/Create the file in "w" (write) mode
    filePointer = fopen(fileName, "w");

    // Always check if the file opened successfully
    if (filePointer == NULL) {
        printf("Error: Could not create or open the file.\n");
        return 1; // Exit the program with an error code
    }

    //Write text to the file
    fprintf(filePointer, "Hello, World!\n");
    fputs("This is another line of text.\n", filePointer);

    printf(fileName, " created successfully.\n");
    return filePointer;
}

static void saveFile(FILE *filePointer){
//Always close the file to save changes and free resources
fclose(filePointer);
}

static void readFile(FILE *filePointer, char fileName[]){
createOrOpenFile(filePointer, fileName);

char readBuffer[100]; //create a buffer to store the current words in a file being read
while(fgets(readBuffer, sizeof(readBuffer), filePointer) != NULL){ // Reads up to 99 characters or until a newline, storing it in readBuffer
printf("%s", readBuffer);
}
}