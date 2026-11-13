#ifndef DEADLOCK_DEMO_H
#define DEADLOCK_DEMO_H

/* Run the six-thread deadlock demo and stream its log to client_fd.
 * mode: "deadlock" | "prevent" | "recover" | "all" (NULL/"" = all).
 * Resets the .txt files and rewrites execution.log. Caller sends .END. */
int run_demo(const char *mode, int client_fd);

#endif