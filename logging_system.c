#include <stdio.h>
#include <stdlib.h>

//loog writer does not need to read the file just write to it and make sure its there. 
FILE *loog;

static FILE* createOrOpenFile(FILE *loog, char fileName[]) {

    //Open/Create the file in "w" (write) mode (overrides whatever written)
    loog = fopen(fileName, "w"); // <-- Fixed typo (changed loog to loog)

    //Always check if the file opened successfully
    if (loog == NULL) {
        printf("Error: Could not create or open loog file.\n");
        return NULL; // Exit the program with an error code
    }

    printf("loog file %s created successfully.\n", fileName);
    return loog;
}
//user has to specify loog file anyways, we shouldnt have to specifiy it each write call too.


static void appendFile(char message[]){ //Appending adds another entry to the end of the file
// Always check if the file opened successfully
    if (loog == NULL) {
        printf("Error: You did not specify a log file.\n");
    }

    //Write text to the file
    fputs(message, loog);
}

static void CloseReader(){
    fclose(loog); 
}
