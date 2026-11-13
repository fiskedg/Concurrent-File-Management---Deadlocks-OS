#define _POSIX_C_SOURCE 200809L   // localtime_r, clock_gettime
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include "logging_system.h"

static FILE *loog = NULL;   // log file pointer, NULL if not open
static int mirror_fd = -1;  

//we need syncronysed log access, here's a mutex to ensure that only one thread can write to the log at a time

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int log_open(const char fileName[]) {

    pthread_mutex_lock(&log_mutex); 
    loog = fopen(fileName, "w");   // "w": fresh log each server run
    pthread_mutex_unlock(&log_mutex);
    if (loog == NULL) {
        perror("log_open");
        return -1;
    }
    return 0;
}


//way better logging function to reflect project requirements.
void log_event(const char *thread, const char *event, const char *resource,
               const char *result, const char *fmt, ...) {
    char details[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(details, sizeof(details), fmt, ap);
    va_end(ap);

    pthread_mutex_lock(&log_mutex); //alright time to write, lock it.

    // timestamp taken inside the lock so lines in the file are in time order
    struct timespec ts;//get the current time with nanosecond precision
    struct tm tm;//convert to local time
    char timeBuf[16];
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm);

    char line[700];
    snprintf(line, sizeof line, "%s.%03ld | %-4s | %-19s | %-5s | %-5s | %s\n",
             timeBuf, ts.tv_nsec / 1000000L, thread, event,
             resource ? resource : "-", result, details);

    FILE *outs[2] = { stdout, loog };
    for (int i = 0; i < 2; i++) {
        if (outs[i] == NULL) continue;
        fputs(line, outs[i]);
        fflush(outs[i]);
    }
    // MSG_NOSIGNAL: a client that disconnects mid-demo can't kill the server
    if (mirror_fd >= 0) send(mirror_fd, line, strlen(line), MSG_NOSIGNAL);

    pthread_mutex_unlock(&log_mutex); //im done writing!
}

void log_set_mirror(int fd) {
    pthread_mutex_lock(&log_mutex);
    mirror_fd = fd;
    pthread_mutex_unlock(&log_mutex);
}

void log_close(void) {
    if (loog != NULL) {
        fclose(loog);
        loog = NULL;
    }
}