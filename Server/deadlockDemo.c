/*
 * This is a demonstration of deadlock, prevention, and recovery in a multi-threaded file management system.
 * 
 * Locks A-D are application locks kept in the table below, not OS file
 * locks. A thread that has to wait records which lock it wants and sleeps
 * on a condition variable instead of blocking inside pthread_mutex_lock().
 * That record is the wait-for graph: the main thread reads it, finds
 * T1->T2->T1, and asks a victim to back out.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include "deadlockDemo.h"
#include "logging_system.h"
#include "file_management_system.h"   /* FILE_DIRECTORY */

enum { A, B, C, D, NRES }; //these are the resources, NRES is the number of resources
enum { DEADLOCK, PREVENT, RECOVER }; //states for threads to be in
#define NT    7     // number of threads: 6 workers + main thread 
#define LINES 32    // max lines in a file
#define LEN   128   // max length of a line 

static const char *rname[NRES] = { "A", "B", "C", "D" };
static const char *rfile[NRES] = { "students.txt", "grades.txt", "audit.txt", "backup.txt" };
static const char *tname[NT]   = { "MAIN", "T1", "T2", "T3", "T4", "T5", "T6" };
static int mode;

//
static pthread_mutex_t lm    = PTHREAD_MUTEX_INITIALIZER;  //lock manager mutex: protects owner[], waiting[], abort_req[]
static pthread_cond_t  lm_cv = PTHREAD_COND_INITIALIZER; //lock manager condition variable: used to signal threads waiting for locks and the detector thread
static int owner[NRES];     //thread holding each lock -1 = free
static int waiting[NT];     //lock each thread waits for -1 = none
static int abort_req[NT];  //set to 1 if a thread is requested to abort, 0 otherwise
static int finished;  //number of threads that have finished their work

//aquires a lock 
static int acquire(int t, int r) {
    pthread_mutex_lock(&lm);
    while (owner[r] != -1 && !abort_req[t]) {
        if (waiting[t] != r) {                 
            waiting[t] = r; //record which lock this thread is waiting for
            log_event(tname[t], "LOCK_WAIT", rname[r], "WAIT", "owner=%s", tname[owner[r]]);
            pthread_cond_broadcast(&lm_cv);  //kick the detector thread to check for deadlocks
        }
        pthread_cond_wait(&lm_cv, &lm);
    }
    waiting[t] = -1;
    int rc = abort_req[t] ? -1 : 0; //if the thread was requested to abort, return -1, otherwise return 0
    if (rc == 0) {
        owner[r] = t;
        log_event(tname[t], "LOCK_ACQUIRED", rname[r], "OK", "%s", rfile[r]);
    }
    pthread_mutex_unlock(&lm);
    return rc;
}

//release them all
static void release_all(int t, const char *why) { 
    pthread_mutex_lock(&lm);
    for (int r = NRES - 1; r >= 0; r--)
        if (owner[r] == t) {
            owner[r] = -1; //aaaallll the locks
            log_event(tname[t], "LOCK_RELEASED", rname[r], "OK", "%s", why); //why is the reason for releasing the lock
        }
    abort_req[t] = 0;
    pthread_cond_broadcast(&lm_cv);
    pthread_mutex_unlock(&lm);
}

static void worker_done(void) {
    pthread_mutex_lock(&lm);
    finished++;
    pthread_cond_broadcast(&lm_cv);
    pthread_mutex_unlock(&lm);
}

//this detects a deadlock by scanning the wait-for graph for cycles
//If a cycle is found, it logs the cycle and either stops all threads in the cycle (mode A) or selects one victim to abort (mode B).
static void detector(void) {
    int cycles = 0;
    pthread_mutex_lock(&lm);
    while (finished < 6) {
        for (int s = 1; s < NT; s++) {
            int path[NT]; //to store the path of the cycle
            int n = 0; //number of threads in the cycle
            int t = s; //traverses the wait-for graph starting from thread s
            do {
                if (t < 0 || waiting[t] < 0 || abort_req[t] || n == NT) { //no cycle found, break out of the loop
                    n = 0; 
                    break; 
                } 
                path[n++] = t;
                t = owner[waiting[t]];
            } while (t != s); //do while! this loop continues until we either find a cycle or reach a thread that is not waiting for any lock or has been requested to abort
            if (n == 0) continue;

            char cyc[64] = "", res[16] = "";
            for (int i = 0; i < n; i++) {
                 strcat(cyc, tname[path[i]]); 
                 strcat(cyc, "->"); 
                    }
                strcat(cyc, tname[s]);
                for (int r = 0; r < NRES; r++) 
                for (int i = 0; i < n; i++)
                    if (waiting[path[i]] == r) { if (res[0]) strcat(res, ","); strcat(res, rname[r]); }
            log_event("MAIN", "DEADLOCK_DETECTED", res, "ALERT", "cycle=%s", cyc);

            for (int i = 0; i < n; i++) {            //for all locks held
                char held[16] = "";
                for (int r = 0; r < NRES; r++)
                    if (owner[r] == path[i]) { if (held[0]) strcat(held, ","); strcat(held, rname[r]); }
                int want = waiting[path[i]];
                log_event("MAIN", "WAIT_FOR_EDGE", rname[want], "INFO", "%s holds %s, requests %s (owner=%s)",
                          tname[path[i]], held, rname[want], tname[owner[want]]);
            }

            if (mode == DEADLOCK) {                   /* mode A: stop everyone in the cycle */
                for (int i = 0; i < n; i++) abort_req[path[i]] = 1;
                log_event("MAIN", "DEMO_STOP", res, "ALERT", "mode A: cancelling cycle members so the demo ends");
            } else {                                  /* recovery: one victim, highest id */
                int v = path[0];
                for (int i = 1; i < n; i++) if (path[i] > v) v = path[i];
                abort_req[v] = 1;
                log_event("MAIN", "VICTIM_SELECTED", rname[waiting[v]], "OK", "victim=%s", tname[v]);
            }
            cycles++;
            pthread_cond_broadcast(&lm_cv);
        }
        struct timespec ts; //wait 50ms before checking for deadlocks again                         
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 50000000L;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        pthread_cond_timedwait(&lm_cv, &lm, &ts);
    }
    pthread_mutex_unlock(&lm);
    log_event("MAIN", "DETECTOR_DONE", "-", "OK", "%d deadlock cycle(s) detected", cycles);
}


typedef struct { 
    char l[LINES][LEN]; 
    int n; 
} buf_t;
 

//write the path of a file with the given extension to p, using FILE_DIRECTORY as the base directory
static void path_of(char *p, const char *f, const char *ext) {
    snprintf(p, 256, "%s%s%s", FILE_DIRECTORY, f, ext);
}
//load the contents of a file into a buffer, stripping newlines and counting non-empty lines
static int load(const char *f, buf_t *b) {
    char p[256];
    path_of(p, f, "");
    FILE *fp = fopen(p, "r");
    if (!fp) return -1;
    b->n = 0;
    while (b->n < LINES && fgets(b->l[b->n], LEN, fp)) {
        b->l[b->n][strcspn(b->l[b->n], "\r\n")] = '\0';
        if (b->l[b->n][0]) b->n++;
    }
    fclose(fp);
    return 0;
}

//write the contents of a buffer to a temporary file
static int stage(const char *f, const buf_t *b) {
    char p[256];
    path_of(p, f, ".tmp");
    FILE *fp = fopen(p, "w");
    if (!fp) return -1;
    for (int i = 0; i < b->n; i++) fprintf(fp, "%s\n", b->l[i]);
    return fclose(fp);
}

//commit the contents of a temporary file to the actual file
static int commit(const char *f) {
    char tmp[256], p[256];
    path_of(tmp, f, ".tmp");
    path_of(p, f, "");
    return rename(tmp, p);
}
//append a line to a file, creating it if it doesn't exist
static int append(const char *f, const char *line) {
    char p[256];
    path_of(p, f, "");
    FILE *fp = fopen(p, "a");
    if (!fp) return -1;
    fprintf(fp, "%s\n", line);
    return fclose(fp);
}

static int replace(buf_t *b, const char *old, const char *new_) {   /* update, not append */
    for (int i = 0; i < b->n; i++)
        if (strcmp(b->l[i], old) == 0) { snprintf(b->l[i], LEN, "%s", new_); return 1; }
    return 0;
}

static void today(char *out) {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(out, 16, "%Y-%m-%d", &tm);
}

// worker threads
static pthread_barrier_t pair;   /* T1 holds A and T2 holds B before either asks for more */

static pthread_mutex_t core = PTHREAD_MUTEX_INITIALIZER;   /* T4-T6 wait for T1/T2 */
static pthread_cond_t  core_cv = PTHREAD_COND_INITIALIZER;
static int core_left, committed[3];

static void core_done(int t, int ok) {
    pthread_mutex_lock(&core);
    committed[t] = ok;
    core_left--;
    pthread_cond_broadcast(&core_cv);
    pthread_mutex_unlock(&core);
}

static void wait_core(void) {
    pthread_mutex_lock(&core);
    while (core_left > 0) pthread_cond_wait(&core_cv, &core);
    pthread_mutex_unlock(&core);
}

static void start(int t, const char *task) {
    log_event(tname[t], "THREAD_START", "-", "OK", "pthread=%lu task=%s",
              (unsigned long)pthread_self(), task);
}

/* T1: read S101, append its Assignment5 grade. A then B. Never the victim in
 * recovery mode (victim = highest id), so it has no retry path. */
static void *t1(void *x) {
    (void)x;
    buf_t s;
    int ok = 0, have = 0;
    start(1, "read S101, append S101,Assignment5,95 (A then B)");
    acquire(1, A);
    if (load("students.txt", &s) == 0)
        for (int i = 0; i < s.n; i++)
            if (strncmp(s.l[i], "S101,", 5) == 0) { have = 1; log_event("T1", "FILE_READ", "A", "OK", "found %s", s.l[i]); }
    if (!have) log_event("T1", "FILE_READ", "A", "ERROR", "S101 not found");

    if (mode != PREVENT) pthread_barrier_wait(&pair);

    if (acquire(1, B) != 0) {
        log_event("T1", "ABORT_REQUESTED", "B", "OK", "victim=T1");
        log_event("T1", "ROLLBACK_COMPLETE", "files", "OK", "nothing written");
    } else if (have && append("grades.txt", "S101,Assignment5,95") == 0) {
        ok = 1;
        log_event("T1", "OP_COMPLETE", "files", "OK", "grade committed");
    }
    release_all(1, ok ? "transaction ended" : "aborted, no changes");
    if (!ok && mode == DEADLOCK) log_event("T1", "SAFE_ABORT", "A,B", "OK", "files unchanged");
    core_done(1, ok);
    worker_done();
    return NULL;
}

static int t2_stage_grade(buf_t *g) {
    if (load("grades.txt", g) == 0 && replace(g, "S102,Assignment4,91", "S102,Assignment4,94")) return 1;
    log_event("T2", "FILE_UPDATE", "B", "ERROR", "S102,Assignment4,91 not found");
    return 0;
}

/* Called with A and B held. Stage both files before renaming either, so a
 * failure leaves both untouched instead of one updated and one stale. */
static int t2_commit(const buf_t *g) {
    buf_t s;
    if (load("students.txt", &s) != 0 || !replace(&s, "S102,Ben,CSC3320", "S102,Benjamin,CSC3320") ||
        stage("students.txt", &s) || stage("grades.txt", g) ||
        commit("students.txt") || commit("grades.txt")) {
        log_event("T2", "FILE_UPDATE", "A,B", "ERROR", "update failed, files not changed");
        return 0;
    }
    log_event("T2", "OP_COMPLETE", "files", "OK", "records updated");
    return 1;
}

/* T2: grade 91->94 and Ben->Benjamin. B then A, except in prevention mode
 * and on retry, where it uses A then B. */
static void *t2(void *x) {
    (void)x;
    buf_t g;
    int ok = 0, ordered = (mode == PREVENT);
    start(2, ordered ? "update S102 grade + name (A then B)" : "update S102 grade + name (B then A)");

    if (!ordered) {
        acquire(2, B);
        int staged = t2_stage_grade(&g);
        if (staged) log_event("T2", "CHANGE_STAGED", "B", "OK", "91->94 held in memory, not written");
        pthread_barrier_wait(&pair);
        if (acquire(2, A) == 0) {
            ok = staged && t2_commit(&g);
        } else {
            log_event("T2", "ABORT_REQUESTED", "B", "OK", "victim=T2");
            log_event("T2", "ROLLBACK_COMPLETE", "files", "OK", "staged changes discarded");
            release_all(2, "cooperative abort");
            if (mode == DEADLOCK) {
                log_event("T2", "SAFE_ABORT", "A,B", "OK", "files unchanged");
                core_done(2, 0);
                worker_done();
                return NULL;
            }
            log_event("T2", "RETRY_STARTED", "A,B", "OK", "ordered A->B");
            ordered = 1;
        }
    }
    if (ordered) {
        acquire(2, A);
        acquire(2, B);
        ok = t2_stage_grade(&g) && t2_commit(&g);
    }
    release_all(2, "transaction ended");
    core_done(2, ok);
    worker_done();
    return NULL;
}

static void *t3(void *x) {           /* count valid student records */
    (void)x;
    buf_t s;
    int count = 0;
    start(3, "count students (A)");
    acquire(3, A);
    if (load("students.txt", &s) == 0) {
        for (int i = 0; i < s.n; i++) {
            int commas = 0;
            for (char *c = s.l[i]; *c; c++) commas += (*c == ',');
            count += (s.l[i][0] == 'S' && commas == 2);
        }
        log_event("T3", "FILE_READ", "A", "OK", "Total students = %d", count);
    } else {
        log_event("T3", "FILE_READ", "A", "ERROR", "students.txt missing");
    }
    release_all(3, "read done");
    worker_done();
    return NULL;
}

static void *t4(void *x) {           /* audit only operations that committed */
    (void)x;
    const char *msg[3] = { "", "T1,Grade added for S101", "T2,Student record updated for S102" };
    char date[16], line[LEN];
    int wrote = 0;
    start(4, "append committed operations to audit.txt (C)");
    wait_core();
    today(date);
    for (int t = 1; t <= 2; t++) {
        if (!committed[t]) continue;
        snprintf(line, sizeof line, "%s,%s", date, msg[t]);
        acquire(4, C);
        if (append("audit.txt", line) == 0) { wrote++; log_event("T4", "AUDIT_APPEND", "C", "OK", "%s", line); }
        release_all(4, "audit written");
    }
    if (!wrote) log_event("T4", "AUDIT_APPEND", "C", "OK", "no committed operations to record");
    worker_done();
    return NULL;
}

static void *t5(void *x) {           //grade statistics: count, average, min, max
    (void)x;
    buf_t g;
    start(5, "grade statistics (B)");
    wait_core();
    acquire(5, B);
    if (load("grades.txt", &g) == 0 && g.n > 0) {
        int sum = 0, mn = 1000, mx = -1;
        for (int i = 0; i < g.n; i++) {
            char *c = strrchr(g.l[i], ',');
            int v = c ? atoi(c + 1) : 0;
            sum += v;
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        log_event("T5", "STATS", "B", "OK", "count=%d average=%.2f min=%d max=%d",
                  g.n, (double)sum / g.n, mn, mx);
    } else {
        log_event("T5", "STATS", "B", "ERROR", "grades.txt missing or empty");
    }
    release_all(5, "read done");
    worker_done();
    return NULL;
}

static void *t6(void *x) {           /* consistent backup: A then B then D */
    (void)x;
    buf_t s, g, out = { .n = 0 };
    start(6, "backup snapshot (A then B then D)");
    wait_core();
    acquire(6, A);
    acquire(6, B);
    acquire(6, D);
    if (load("students.txt", &s) == 0 && load("grades.txt", &g) == 0) {
        snprintf(out.l[out.n++], LEN, "BACKUP - STUDENT RECORDS");
        for (int i = 0; i < s.n; i++) memcpy(out.l[out.n++], s.l[i], LEN);
        snprintf(out.l[out.n++], LEN, "BACKUP - GRADES");
        for (int i = 0; i < g.n; i++) memcpy(out.l[out.n++], g.l[i], LEN);
        if (stage("backup.txt", &out) == 0 && commit("backup.txt") == 0)
            log_event("T6", "BACKUP_CREATED", "D", "OK", "%d student + %d grade records", s.n, g.n);
    } else {
        log_event("T6", "BACKUP", "D", "ERROR", "source file missing");
    }
    release_all(6, "backup done");
    worker_done();
    return NULL;
}

// emit messages to stdout and demo client socket

static int demo_fd = -1;   /* client socket for banners / file dumps */

static void emit(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void emit(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    fputs(buf, stdout);
    if (demo_fd >= 0) send(demo_fd, buf, strlen(buf), MSG_NOSIGNAL);
}

static void reset_file(const char *f, const char *text) {
    char p[256];
    path_of(p, f, "");
    FILE *fp = fopen(p, "w");
    if (fp) { fputs(text, fp); fclose(fp); }
}

static void run_mode(int m) {
    static const char *names[] = { "A-DEADLOCK", "B-PREVENTION", "C-RECOVERY" };
    void *(*fn[6])(void *) = { t1, t2, t3, t4, t5, t6 };
    pthread_t th[6];
    char date[16], audit[64];

    /* fresh data so every run starts the same */
    mkdir(FILE_DIRECTORY, 0755);
    today(date);
    snprintf(audit, sizeof audit, "%s,Project initialized\n", date);
    reset_file("students.txt", "S101,Ana,CSC3320\nS102,Ben,CSC3320\n");
    reset_file("grades.txt", "S101,Assignment4,88\nS102,Assignment4,91\n");
    reset_file("audit.txt", audit);
    reset_file("backup.txt", "");

    mode = m;
    for (int r = 0; r < NRES; r++) owner[r] = -1;
    for (int t = 0; t < NT; t++) { waiting[t] = -1; abort_req[t] = 0; }
    finished = 0;
    core_left = 2;
    committed[1] = committed[2] = 0;
    pthread_barrier_init(&pair, NULL, 2);

    emit("\n================ MODE %s ================\n", names[m]);
    log_event("MAIN", "MODE_START", "-", "OK", "%s, data reset", names[m]);
    for (int i = 0; i < 6; i++) pthread_create(&th[i], NULL, fn[i], NULL);
    detector();
    for (int i = 0; i < 6; i++) pthread_join(th[i], NULL);
    pthread_barrier_destroy(&pair);
    log_event("MAIN", "MODE_END", "-", "OK", "%s, all 6 workers joined", names[m]);

    emit("\n---------------- final file contents ----------------\n");
    for (int r = 0; r < NRES; r++) {
        buf_t b;
        emit("%s:\n", rfile[r]);
        if (load(rfile[r], &b) == 0)
            for (int i = 0; i < b.n; i++) emit("  %s\n", b.l[i]);
    }
    emit("-----------------------------------------------------\n");
}

int run_demo(const char *arg, int client_fd) {
    static pthread_mutex_t running = PTHREAD_MUTEX_INITIALIZER;   /* one demo at a time */
    int m;

    if (arg == NULL || *arg == '\0' || strcasecmp(arg, "all") == 0) m = -1;
    else if (strcasecmp(arg, "deadlock") == 0) m = DEADLOCK;
    else if (strcasecmp(arg, "prevent")  == 0) m = PREVENT;
    else if (strcasecmp(arg, "recover")  == 0) m = RECOVER;
    else { dprintf(client_fd, "Usage: DEMO <deadlock|prevent|recover|all>\n"); return -1; }

    if (pthread_mutex_trylock(&running) != 0) {
        dprintf(client_fd, "Error: a demo is already running.\n");
        return -1;
    }
    demo_fd = client_fd;
    log_open("execution.log");
    log_set_mirror(client_fd);

    if (m >= 0) run_mode(m);
    else for (int i = DEADLOCK; i <= RECOVER; i++) run_mode(i);

    log_set_mirror(-1);
    log_close();
    demo_fd = -1;
    pthread_mutex_unlock(&running);
    return 0;
}