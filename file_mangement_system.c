/*
 * Compile and run instructions:
 *
 * 1. Compile:
 *    gcc file_mangement_system.c -pthread -o file_management_system
 *
 * 2. Run:
 *    ./file_management_system
 *
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#define STUDENT_FILE "students.txt"
#define GRADE_FILE "grades.txt"
#define AUDIT_FILE "audit.txt"
#define BACKUP_FILE "backup.txt"
#define TEMP_FILE "temp.txt"

typedef enum {
    DEADLOCK_DEMO = 0,
    PREVENTION_MODE = 1
} LockMode;

static LockMode currentMode = DEADLOCK_DEMO;

static pthread_mutex_t lockA = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lockB = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lockC = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lockD = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t demoGate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t demoCond = PTHREAD_COND_INITIALIZER;
static int t1Ready = 0;
static int t2Ready = 0;
static int t1Attempted = 0;
static int t2Attempted = 0;

static FILE* createOrOpenFile(FILE *filePointer, char fileName[]) {
    filePointer = fopen(fileName, "w");
    if (filePointer == NULL) {
        printf("Error: Could not create or open the file %s.\n", fileName);
        return NULL;
    }
    printf("%s created successfully.\n", fileName);
    return filePointer;
}

static void readFile(char fileName[]) {
    FILE *filePointer = fopen(fileName, "r");
    if (filePointer == NULL) {
        printf("Error opening file %s.\n", fileName);
        return;
    }

    char readBuffer[256];
    while (fgets(readBuffer, sizeof(readBuffer), filePointer) != NULL) {
        printf("%s", readBuffer);
    }
    fclose(filePointer);
}

static void appendFile(const char *fileName, const char *message) {
    FILE *filePointer = fopen(fileName, "a");
    if (filePointer == NULL) {
        printf("Error: Could not open the file %s.\n", fileName);
        return;
    }

    fputs(message, filePointer);
    fclose(filePointer);
}

static void updateFile(char fileName[], char oldMessage[], char newMessage[]) {
    FILE *originalFilePointer = fopen(fileName, "r");
    if (originalFilePointer == NULL) {
        printf("Error: Could not open %s.\n", fileName);
        return;
    }

    FILE *tempFilePointer = fopen(TEMP_FILE, "w");
    if (tempFilePointer == NULL) {
        printf("Error creating temporary file.\n");
        fclose(originalFilePointer);
        return;
    }

    char readBuffer[256];
    int updated = 0;

    while (fgets(readBuffer, sizeof(readBuffer), originalFilePointer) != NULL) {
        if (strcmp(readBuffer, oldMessage) == 0) {
            fputs(newMessage, tempFilePointer);
            updated = 1;
        } else {
            fputs(readBuffer, tempFilePointer);
        }
    }

    fclose(originalFilePointer);
    fclose(tempFilePointer);

    if (updated) {
        remove(fileName);
        rename(TEMP_FILE, fileName);
        printf("Record successfully updated in %s.\n", fileName);
    } else {
        remove(TEMP_FILE);
        printf("Record not found in %s.\n", fileName);
    }
}

static void replaceTextInFile(const char *fileName, const char *oldText, const char *newText) {
    FILE *source = fopen(fileName, "r");
    if (source == NULL) {
        printf("Error opening %s for replacement.\n", fileName);
        return;
    }

    FILE *temp = fopen(TEMP_FILE, "w");
    if (temp == NULL) {
        printf("Error creating temporary replacement file.\n");
        fclose(source);
        return;
    }

    char buffer[512];
    int replaced = 0;

    while (fgets(buffer, sizeof(buffer), source) != NULL) {
        char *position = strstr(buffer, oldText);
        if (position != NULL) {
            size_t prefixLength = (size_t)(position - buffer);
            fwrite(buffer, 1, prefixLength, temp);
            fputs(newText, temp);
            fputs(position + strlen(oldText), temp);
            replaced = 1;
        } else {
            fputs(buffer, temp);
        }
    }

    fclose(source);
    fclose(temp);

    if (replaced) {
        remove(fileName);
        rename(TEMP_FILE, fileName);
        printf("Updated %s: %s -> %s\n", fileName, oldText, newText);
    } else {
        remove(TEMP_FILE);
        printf("No match found for %s in %s.\n", oldText, fileName);
    }
}

static void lastModified(char fileName[]) {
    struct stat fileInfo;
    if (stat(fileName, &fileInfo) != 0) {
        printf("Error retrieving file information for %s.\n", fileName);
        return;
    }

    printf("File size: %ld bytes\n", (long)fileInfo.st_size);
    printf("Last modified: %s", ctime(&fileInfo.st_mtime));
}

static void listFiles(char directoryName[]) {
    DIR *currDirectory = opendir(directoryName);
    if (currDirectory == NULL) {
        printf("Error: Could not open the directory %s.\n", directoryName);
        return;
    }

    struct dirent *entry;
    printf("Files in directory:\n");
    while ((entry = readdir(currDirectory)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            printf("%s\n", entry->d_name);
        }
    }
    closedir(currDirectory);
}

static void initializeFiles(void) {
    FILE *students = fopen(STUDENT_FILE, "w");
    FILE *grades = fopen(GRADE_FILE, "w");
    FILE *audit = fopen(AUDIT_FILE, "w");
    FILE *backup = fopen(BACKUP_FILE, "w");

    if (students == NULL || grades == NULL || audit == NULL || backup == NULL) {
        printf("Error initializing project files.\n");
        exit(1);
    }

    fputs("S101,Alice,89\nS102,Ben,91\nS103,Charlie,88\n", students);
    fputs("S101,Assignment1,85\nS102,Assignment4,91\nS103,Assignment2,93\n", grades);
    fclose(students);
    fclose(grades);
    fclose(audit);
    fclose(backup);

    printf("Initial files created with sample data.\n");
}

static void logThreadStatus(const char *threadLabel, const char *message) {
    pthread_mutex_lock(&logMutex);
    printf("[%s tid=%lu] %s\n", threadLabel, (unsigned long)pthread_self(), message);
    pthread_mutex_unlock(&logMutex);
}

static int countValidStudentRecords(const char *fileName) {
    FILE *fp = fopen(fileName, "r");
    if (fp == NULL) {
        printf("Could not open %s for counting records.\n", fileName);
        return 0;
    }

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line[0] == '\n' || line[0] == '\0') {
            continue;
        }
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\r' || *trimmed == '\n') {
            trimmed++;
        }
        if (trimmed[0] == 'S') {
            count++;
        }
    }

    fclose(fp);
    return count;
}

static void calculateGradeStatistics(const char *fileName) {
    FILE *fp = fopen(fileName, "r");
    if (fp == NULL) {
        printf("Could not open %s for grade statistics.\n", fileName);
        return;
    }

    char line[256];
    int count = 0;
    double total = 0.0;
    int minGrade = 9999;
    int maxGrade = -9999;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *token = strtok(line, ",\n\r");
        if (token == NULL) {
            continue;
        }

        token = strtok(NULL, ",\n\r");
        if (token == NULL) {
            continue;
        }

        token = strtok(NULL, ",\n\r");
        if (token == NULL) {
            continue;
        }

        int grade = atoi(token);
        count++;
        total += grade;
        if (grade < minGrade) {
            minGrade = grade;
        }
        if (grade > maxGrade) {
            maxGrade = grade;
        }
    }

    fclose(fp);

    printf("[T5] Grade statistics: records=%d, mean=%.2f, min=%d, max=%d\n",
           count, (count == 0) ? 0.0 : total / (double)count, minGrade == 9999 ? 0 : minGrade,
           maxGrade == -9999 ? 0 : maxGrade);
}

static void writeBackupSnapshot(void) {
    FILE *students = fopen(STUDENT_FILE, "r");
    FILE *grades = fopen(GRADE_FILE, "r");
    FILE *backup = fopen(BACKUP_FILE, "w");

    if (students == NULL || grades == NULL || backup == NULL) {
        printf("Could not create backup snapshot.\n");
        if (students != NULL) fclose(students);
        if (grades != NULL) fclose(grades);
        if (backup != NULL) fclose(backup);
        return;
    }

    char buffer[256];
    fputs("=== Student Snapshot ===\n", backup);
    while (fgets(buffer, sizeof(buffer), students) != NULL) {
        fputs(buffer, backup);
    }
    fputs("=== Grade Snapshot ===\n", backup);
    while (fgets(buffer, sizeof(buffer), grades) != NULL) {
        fputs(buffer, backup);
    }

    fclose(students);
    fclose(grades);
    fclose(backup);
    printf("[T6] Backup snapshot written to %s\n", BACKUP_FILE);
}

static void appendAuditEntry(const char *entry) {
    appendFile(AUDIT_FILE, entry);
}

static void* worker_t1(void *arg) {
    (void)arg;
    logThreadStatus("T1", "Starting: reading student records and preparing grade append.");

    if (currentMode == DEADLOCK_DEMO) {
        pthread_mutex_lock(&lockA);
        pthread_mutex_lock(&demoGate);
        t1Ready = 1;
        pthread_cond_broadcast(&demoCond);
        while (t2Ready == 0) {
            pthread_cond_wait(&demoCond, &demoGate);
        }
        pthread_mutex_unlock(&demoGate);

        logThreadStatus("T1", "Acquired lock A (students.txt) and is now attempting lock B while T2 holds B.");
        if (pthread_mutex_trylock(&lockB) != 0) {
            logThreadStatus("T1", "Deadlock demo: circular wait detected (T1 holds A, requests B).");
            pthread_mutex_lock(&demoGate);
            t1Attempted = 1;
            pthread_cond_broadcast(&demoCond);
            while (t2Attempted == 0) {
                pthread_cond_wait(&demoCond, &demoGate);
            }
            pthread_mutex_unlock(&demoGate);
            pthread_mutex_unlock(&lockA);
            return NULL;
        }
        pthread_mutex_unlock(&lockB);
        pthread_mutex_unlock(&lockA);
    } else {
        pthread_mutex_lock(&lockA);
        pthread_mutex_lock(&lockB);
        logThreadStatus("T1", "Acquired locks A then B. Appending grade entry to grades.txt.");
        appendFile(GRADE_FILE, "S101,Assignment5,95\n");
        pthread_mutex_unlock(&lockB);
        pthread_mutex_unlock(&lockA);
    }

    char studentData[256];
    FILE *studentFile = fopen(STUDENT_FILE, "r");
    if (studentFile != NULL) {
        if (fgets(studentData, sizeof(studentData), studentFile) != NULL) {
            logThreadStatus("T1", "Student data read successfully before grade append.");
        }
        fclose(studentFile);
    }

    if (currentMode == DEADLOCK_DEMO) {
        appendFile(GRADE_FILE, "S101,Assignment5,95\n");
        logThreadStatus("T1", "Grade append executed after controlled deadlock release.");
    }

    return NULL;
}

static void* worker_t2(void *arg) {
    (void)arg;
    logThreadStatus("T2", "Starting: updating grade and student record.");

    if (currentMode == DEADLOCK_DEMO) {
        pthread_mutex_lock(&lockB);
        pthread_mutex_lock(&demoGate);
        t2Ready = 1;
        pthread_cond_broadcast(&demoCond);
        while (t1Ready == 0) {
            pthread_cond_wait(&demoCond, &demoGate);
        }
        pthread_mutex_unlock(&demoGate);

        logThreadStatus("T2", "Acquired lock B (grades.txt) and is now attempting lock A while T1 holds A.");
        if (pthread_mutex_trylock(&lockA) != 0) {
            logThreadStatus("T2", "Deadlock demo: circular wait detected (T2 holds B, requests A).");
            pthread_mutex_lock(&demoGate);
            t2Attempted = 1;
            pthread_cond_broadcast(&demoCond);
            while (t1Attempted == 0) {
                pthread_cond_wait(&demoCond, &demoGate);
            }
            pthread_mutex_unlock(&demoGate);
            pthread_mutex_unlock(&lockB);
            return NULL;
        }
        pthread_mutex_unlock(&lockA);
        pthread_mutex_unlock(&lockB);
    } else {
        pthread_mutex_lock(&lockA);
        pthread_mutex_lock(&lockB);
        logThreadStatus("T2", "Acquired locks A then B (prevention mode) and is updating files.");
        replaceTextInFile(GRADE_FILE, "S102,Assignment4,91", "S102,Assignment4,94");
        replaceTextInFile(STUDENT_FILE, "Ben", "Benjamin");
        pthread_mutex_unlock(&lockB);
        pthread_mutex_unlock(&lockA);
    }

    if (currentMode == DEADLOCK_DEMO) {
        replaceTextInFile(GRADE_FILE, "S102,Assignment4,91", "S102,Assignment4,94");
        replaceTextInFile(STUDENT_FILE, "Ben", "Benjamin");
        logThreadStatus("T2", "Student and grade update completed after controlled deadlock release.");
    }

    return NULL;
}

static void* worker_t3(void *arg) {
    (void)arg;
    logThreadStatus("T3", "Starting: counting student records.");
    pthread_mutex_lock(&lockA);
    logThreadStatus("T3", "Acquired lock A.");

    int count = countValidStudentRecords(STUDENT_FILE);
    printf("[T3] Valid student records: %d\n", count);

    pthread_mutex_unlock(&lockA);
    return NULL;
}

static void* worker_t4(void *arg) {
    (void)arg;
    logThreadStatus("T4", "Starting: writing audit entry.");
    pthread_mutex_lock(&lockC);
    logThreadStatus("T4", "Acquired lock C.");
    appendAuditEntry("T4,transaction_completed,2026-11-09\n");
    pthread_mutex_unlock(&lockC);
    return NULL;
}

static void* worker_t5(void *arg) {
    (void)arg;
    logThreadStatus("T5", "Starting: reading grades and calculating statistics.");
    pthread_mutex_lock(&lockB);
    logThreadStatus("T5", "Acquired lock B.");
    calculateGradeStatistics(GRADE_FILE);
    pthread_mutex_unlock(&lockB);
    return NULL;
}

static void* worker_t6(void *arg) {
    (void)arg;
    logThreadStatus("T6", "Starting: creating backup snapshot from consistent view.");
    pthread_mutex_lock(&lockA);
    pthread_mutex_lock(&lockB);
    pthread_mutex_lock(&lockD);
    logThreadStatus("T6", "Acquired locks A->B->D. Writing backup.txt.");
    writeBackupSnapshot();
    pthread_mutex_unlock(&lockD);
    pthread_mutex_unlock(&lockB);
    pthread_mutex_unlock(&lockA);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "prevention") == 0) {
        currentMode = PREVENTION_MODE;
    }

    initializeFiles();
    printf("Mode: %s\n", currentMode == DEADLOCK_DEMO ? "controlled deadlock demo" : "lock-order prevention");

    pthread_t thread1, thread2, thread3, thread4, thread5, thread6;

    pthread_create(&thread1, NULL, worker_t1, NULL);
    pthread_create(&thread2, NULL, worker_t2, NULL);
    pthread_create(&thread3, NULL, worker_t3, NULL);
    pthread_create(&thread4, NULL, worker_t4, NULL);
    pthread_create(&thread5, NULL, worker_t5, NULL);
    pthread_create(&thread6, NULL, worker_t6, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    pthread_join(thread4, NULL);
    pthread_join(thread5, NULL);
    pthread_join(thread6, NULL);

    printf("All worker threads completed successfully.\n");
    return 0;
}