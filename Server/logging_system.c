#define _POSIX_C_SOURCE 200809L   // localtime_r, clock_gettime
//200809L is a macro that enables certain POSIX features in C, such as localtime_r and clock_gettime. It ensures that the code can use these functions for thread-safe time manipulation and high-resolution timing.
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include "logging_system.h"

//loog writer does not need to read the file just write to it and make sure its there.
 FILE *loog = NULL;   // log file pointer, NULL if not open

int log_open(const char fileName[]) {
    loog = fopen(fileName, "w");   // "w": fresh log each server run
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
               
    struct timespec ts; // get current time with nanosecond precision
    struct tm tm; // convert to local time
    char timeBuf[16];
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm);

    // one fprintf per line so a line is written in a single call
    FILE *outs[2] = { stdout, loog }; // write to both stdout and log file (if open)
    for (int i = 0; i < 2; i++) {
        if (outs[i] == NULL) continue;
        fprintf(outs[i], "%s.%03ld | %s | %s | %s | %s | %s\n",
                timeBuf, ts.tv_nsec / 1000000L, thread, event,
                resource ? resource : "-", result, details);
        fflush(outs[i]);
    }
    // TODO (locking group): unlock log mutex here
}

void log_close(void) {
    if (loog != NULL) {
        fclose(loog);
        loog = NULL;
    }
}