#ifndef LOGGING_SYSTEM_H
#define LOGGING_SYSTEM_H

/*
 * Execution log, one line per event:
 *   HH:MM:SS.mmm | thread | event | resource | result | details
 */

int  log_open(const char fileName[]);   // 0 = ok, -1 = couldn't open (still logs to stdout)
void log_event(const char *thread, const char *event, const char *resource,
               const char *result, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));
void log_close(void);

#endif