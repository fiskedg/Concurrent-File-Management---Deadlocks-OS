#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

static FILE* createOrOpenFile(FILE *filePointer, char fileName[]) {

    //Open/Create the file in "w" (write) mode (overrides whatever written)
    filePointer = fopen(fileName, "w");

    // Always check if the file opened successfully
    if (filePointer == NULL) {
        printf("Error: Could not create or open the file.\n");
        return NULL; // Exit the program with an error code
    }

    printf("%s created successfully.\n", fileName);
    return filePointer;
}

static void readFile(char fileName[]){
FILE *filePointer = fopen(fileName, "r"); //open a file in read only mode

    if (filePointer == NULL) {
        printf("Error opening file.\n");
        return;
    }


char readBuffer[100]; //create a buffer to store the current words in a file being read
while(fgets(readBuffer, sizeof(readBuffer), filePointer) != NULL){ // Reads up to 99 characters or until a newline, storing it in readBuffer
printf("%s", readBuffer);
}
fclose(filePointer);
}

static void appendFile(char fileName[], char message[]){ //Appending adds another entry to the end of the file
    FILE *filePointer = fopen(fileName, "a"); //open a file in append mode (doesn't overwrite what has already been written)

// Always check if the file opened successfully
    if (filePointer == NULL) {
        printf("Error: Could notopen the file.\n");
        return; // Exit the program with an error code
    }

    //Write text to the file
    if (fputs(message, filePointer) == EOF) {
        printf("Error writing to file.\n");
    }

    fclose(filePointer);
}

static void updateFile(char fileName[], char oldMessage[], char newMessage[]){ //Updating rewrites preexisting entries
    FILE *originalFilePointer = fopen(fileName, "r"); //open a file in write mode (overwrites what has already been written)

FILE *tempFilePointer = fopen("temp.txt", "w");

    if (tempFilePointer == NULL) {
        printf("Error creating temporary file.\n");
        fclose(originalFilePointer);
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
        if (rename("temp.txt", fileName) != 0) {
            printf("Error replacing original file.\n");
            remove("temp.txt");
            return;
        }

    printf("Record successfully updated.\n");
}else{
    printf("Record not found. No changes were made.\n");
    remove("temp.txt");
}
}


static void lastModified(char fileName[]){
struct stat fileInfo;

    if (stat(fileName, &fileInfo) != 0) {
        printf("Error retrieving file information.\n");
        return;
    }

   printf("File size: %ld bytes\n", (long)fileInfo.st_size);
   printf("Last modified: %s", ctime(&fileInfo.st_mtime));

}

static void listFiles(char directoryName[]){
DIR *currDirectory = opendir(directoryName);

// Always check if the directory opened successfully
    if (currDirectory == NULL) {
        printf("Error: Could not open the directory.\n");
        return; // Exit the program with an error code
    }

    struct dirent *entry; //Directory entry

    printf("Files in directory:\n");
    while((entry =readdir(currDirectory)) != NULL){
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) { //d_name = actual name of the file, entry->d_name allows access to that name starting from the pointer

            printf("%s\n", entry->d_name);
        }
    }
    closedir(currDirectory);
}

int main() {

    // 1. Test creating a new file
    FILE *testFile = createOrOpenFile(NULL, "created_test.txt");

    if (testFile != NULL) {
        fputs("This is a test file.\n", testFile);
        fclose(testFile);
    }

    // 2. Test reading an existing file
    printf("\n--- Testing readFile ---\n");
    readFile("students.txt");

    // 3. Test appending a new record
    printf("\n--- Testing appendFile ---\n");
    appendFile("students.txt", "Alex, 88\n");

    // 4. Test updating an existing record
    printf("\n--- Testing updateFile ---\n");
    updateFile("students.txt", "John, 85\n", "John, 95\n");

    // 5. Test retrieving file information
    printf("\n--- Testing lastModified ---\n");
    lastModified("students.txt");

    // 6. Test listing files in the current directory
    printf("\n--- Testing listFiles ---\n");
    listFiles(".");

    return 0;
}