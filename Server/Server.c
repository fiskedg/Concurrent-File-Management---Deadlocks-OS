#define _GNU_SOURCE   // dprintf, strtok_r
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "file_management_system.h"
#include "logging_system.h"

<<<<<<< HEAD
#define QUEUE_SIZE 10
#define NUM_WORKERS 4
=======
#define QUEUE_SIZE 10 
#define NUM_WORKERS 6
>>>>>>> 12a72b15a862bf4c166daad52c5d5f52c8dbc414
#define PORT 5000
#define BUFFER_SIZE 4096
#define MAX_FILENAME 200
#define LOG_FILE "execution.log"


int process_command(char *command, int client_fd);

typedef struct {
    int client_fd;
} task_t;

typedef struct {
    task_t queue[QUEUE_SIZE];
    int front;
    int rear;
    int count;
    int shutdown;
    pthread_mutex_t mutex;   // Mutex for synchronizing access to the queue
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    pthread_t workers[NUM_WORKERS];
} thread_pool_t;

typedef struct {
    thread_pool_t *pool;
    int id;   // 1 to NUM_WORKERS
} worker_arg_t; // Arguments passed to each worker thread

static worker_arg_t worker_args[NUM_WORKERS]; // Pre-allocated worker arguments for each thread

// Each thread's name for the log: "MAIN" for the accept loop, "T1".."T4" for workers
static __thread char thread_name[8] = "MAIN";

//mutex logic goes here
static void file_lock_acquire(const char *fileName) {
    (void)fileName;
}

static void file_lock_release(const char *fileName) {
    (void)fileName;
}



// Only allow plain names like "students.txt": no paths, no "..", no hidden files
static int valid_filename(const char *name) {
    if (name == NULL || name[0] == '\0' || name[0] == '.') return 0;
    if (strlen(name) > MAX_FILENAME) {
        return 0;
    }
    if (strchr(name, '/') != NULL) {
        return 0;
    }
    return 1;
}

// Splits "word rest of line" into word + rest. Returns rest (or NULL if none).
static char *split_first(char *s) {
    char *sp = strchr(s, ' ');
    if (sp == NULL) return NULL;
    *sp = '\0';
    sp++;
    while (*sp == ' ') sp++;
    return (*sp == '\0') ? NULL : sp;
}

static void reply_end(int client_fd) {
    dprintf(client_fd, ".END.\n");
}



//good ol client, each worker thread 1 per job
void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE]; // Local thread-safe buffer
    ssize_t n; // Number of bytes read
    int quit = 0; //does the user want out

    log_event(thread_name, "CLIENT_CONNECTED", "-", "OK", "fd=%d", client_fd);

    while (!quit && (n = read(client_fd, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[n] = '\0';
        // a single read can contain several commands, handle each line
        char *save = NULL;
        for (char *line = strtok_r(buffer, "\r\n", &save);
             line != NULL && !quit;
             line = strtok_r(NULL, "\r\n", &save)) {
            quit = process_command(line, client_fd);
        }
    }
    if (n == -1) {
        perror("read");
    }

    log_event(thread_name, "CLIENT_DISCONNECTED", "-", "OK", "fd=%d", client_fd);
    close(client_fd);
}

//this is the worker function, lowlevel worker logic
void *worker(void *arg) {
    worker_arg_t *wa = (worker_arg_t *)arg;
    thread_pool_t *pool = wa->pool;
    snprintf(thread_name, sizeof(thread_name), "T%d", wa->id);

    while (1) {
        task_t task;

        pthread_mutex_lock(&pool->mutex); //lock mutex and wait for job
        while (pool->count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->not_empty, &pool->mutex);
        }

        if (pool->count == 0 && pool->shutdown) { //shutdown condition
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        task = pool->queue[pool->front];
        pool->front = (pool->front + 1) % QUEUE_SIZE; //queue is circular, so we wrap around
        pool->count--;

        pthread_cond_signal(&pool->not_full);
        pthread_mutex_unlock(&pool->mutex);

        handle_client(task.client_fd); //execute the job
    }
    return NULL;
}

//initializes the thread pool and creates the worker threads
void pool_init(thread_pool_t *pool) {
    pool->front = 0;
    pool->rear = 0;
    pool->count = 0;
    pool->shutdown = 0;

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->not_empty, NULL);
    pthread_cond_init(&pool->not_full, NULL);

    for (int i = 0; i < NUM_WORKERS; i++) {
        worker_args[i].pool = pool;
        worker_args[i].id = i + 1;
        pthread_create(&pool->workers[i], NULL, worker, &worker_args[i]);
    }
}

//thread safe enqueue
void pool_push(thread_pool_t *pool, int client_fd) {
    pthread_mutex_lock(&pool->mutex);

    while (pool->count == QUEUE_SIZE && !pool->shutdown) {
        pthread_cond_wait(&pool->not_full, &pool->mutex);
    }

    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->mutex);
        close(client_fd);
        return;
    }

    pool->queue[pool->rear].client_fd = client_fd;
    pool->rear = (pool->rear + 1) % QUEUE_SIZE;
    pool->count++;

    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->mutex);
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);   // client vanishing mid-reply shouldn't kill the server

    if (mkdir(FILE_DIRECTORY, 0755) != 0 && errno != EEXIST) {
        perror("mkdir " FILE_DIRECTORY);
        return 1;
    }

    log_open(LOG_FILE);

    thread_pool_t pool;
    pool_init(&pool);

    int server_fd, client_fd;
    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr)); //we have to clear the struct before we use it, otherwise it will have garbage values
    server_addr.sin_family = AF_INET;          // Use IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;  //listen on all interfaces
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Bootleg Google Docs running on port %d...\n", PORT);
    log_event(thread_name, "SERVER_START", "-", "OK",
              "port=%d workers=%d dir=%s", PORT, NUM_WORKERS, FILE_DIRECTORY);

    while (1) {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }
 //delagate client management to worker queue  
        pool_push(&pool, client_fd); 
    }

    close(server_fd);
    log_close();
    return 0;
}




// Logs the result of a file operation and tells the client if it failed
static void finish_op(int rc, const char *op, const char *file, int client_fd) {
    if (rc < 0) {
        int err = errno;
        log_event(thread_name, "OP_FAILED", file, "ERROR", "%s: %s", op, strerror(err));
        dprintf(client_fd, "Error: %s %s failed: %s\n", op, file, strerror(err)); //tell the client what went wrong
    } else {
        log_event(thread_name, "OP_COMPLETE", file, "OK", "%s", op);  
    }
}

// Returns 1 if the client asked to QUIT
int process_command(char *command, int client_fd) {
    printf("[%s] Requested: %s\n", thread_name, command);

    char *argument = split_first(command);   // command = verb, argument = everything after it

    // These don't touch files
    if (strcmp(command, "QUIT") == 0) {
        dprintf(client_fd, "Goodbye\n");
        reply_end(client_fd);
        return 1;
    }
    if (strcmp(command, "HELP") == 0) {
        send_help(client_fd);
        reply_end(client_fd);
        return 0;
    }
    if (strcmp(command, "LIST") == 0) {
        int rc = listFiles(FILE_DIRECTORY, client_fd);  //
        finish_op(rc, "list", FILE_DIRECTORY, client_fd);
        reply_end(client_fd);
        return 0;
    }

    // Everything else is "<VERB> <file> [extra]"
    int known = strcmp(command, "CREATE") == 0 || strcmp(command, "READ") == 0 ||
                strcmp(command, "APPEND") == 0 || strcmp(command, "UPDATE") == 0 ||
                strcmp(command, "INFO") == 0;
    if (!known) {
        dprintf(client_fd, "Unknown command: %s (try HELP)\n", command);
        reply_end(client_fd);
        return 0;
    }
    if (argument == NULL) {
        dprintf(client_fd, "Error: No file name provided.\n");
        reply_end(client_fd);
        return 0;
    }

    char *fileName = argument;
    char *extra = split_first(fileName);   // text for APPEND / "old|new" for UPDATE

    if (!valid_filename(fileName)) {
        dprintf(client_fd, "Error: Invalid file name '%s' (no paths or leading dots).\n", fileName);
        reply_end(client_fd);
        return 0;
    }

    if (strcmp(command, "CREATE") == 0) {
        file_lock_acquire(fileName);
        FILE *newFile = createOrOpenFile(NULL, fileName);
        int rc = (newFile == NULL) ? -1 : 0;
        if (newFile != NULL) fclose(newFile);
        file_lock_release(fileName);

        finish_op(rc, "create", fileName, client_fd);
        if (rc == 0) dprintf(client_fd, "File ready: %s\n", fileName);

    } else if (strcmp(command, "READ") == 0) {
        file_lock_acquire(fileName);
        int rc = readFile(fileName, client_fd);   // file contents go straight to the client
        file_lock_release(fileName);

        finish_op(rc, "read", fileName, client_fd);

    } else if (strcmp(command, "APPEND") == 0) {
        if (extra == NULL) {
            dprintf(client_fd, "Usage: APPEND <file> <text>\n");
        } else {
            char line[BUFFER_SIZE];
            snprintf(line, sizeof(line), "%s\n", extra);

            file_lock_acquire(fileName);
            int rc = appendFile(fileName, line);
            file_lock_release(fileName);

            finish_op(rc, "append", fileName, client_fd);
            if (rc == 0) dprintf(client_fd, "Appended to %s\n", fileName);
        }

    } else if (strcmp(command, "UPDATE") == 0) {
        char *bar = extra ? strchr(extra, '|') : NULL;
        if (bar == NULL) {
            dprintf(client_fd, "Usage: UPDATE <file> <old line>|<new line>\n");
        } else {
            *bar = '\0';
            char oldLine[BUFFER_SIZE], newLine[BUFFER_SIZE];
            snprintf(oldLine, sizeof(oldLine), "%s\n", extra);    // file lines end in '\n'
            snprintf(newLine, sizeof(newLine), "%s\n", bar + 1);

            file_lock_acquire(fileName);
            int rc = updateFile(fileName, oldLine, newLine);
            file_lock_release(fileName);

            if (rc == 1) {
                log_event(thread_name, "OP_COMPLETE", fileName, "OK", "update committed");
                dprintf(client_fd, "Record updated in %s\n", fileName);
            } else if (rc == 0) {
                log_event(thread_name, "OP_COMPLETE", fileName, "NOT_FOUND", "no matching line");
                dprintf(client_fd, "Record not found. No changes were made.\n");
            } else {
                finish_op(rc, "update", fileName, client_fd);
            }
        }

    } else if (strcmp(command, "INFO") == 0) {
        file_lock_acquire(fileName);
        int rc = lastModified(fileName, client_fd);
        file_lock_release(fileName);

        finish_op(rc, "info", fileName, client_fd);
    }

    reply_end(client_fd);
    return 0;
            fclose(newFile);
            dprintf(client_fd, "File created successfully: %s\n.END.\n", argument);
        }
       
    } else if (strncmp(command, "READ", 4) == 0) {
        // Handle READ command
        if (argument == NULL || strlen(argument) == 0) {
            dprintf(client_fd, "Error: No file name provided.\n.END.\n");
            return;
        }
        readFile(argument, client_fd);
        dprintf(client_fd, "\n.END.\n");
    } else if (strncmp(command, "APPEND", 6) == 0) {
        // Handle APPEND command
        if (argument == NULL || strlen(argument) == 0) {
            dprintf(client_fd, "Error: No file name provided.\n.END.\n");
            return;
        }
        char *message = strchr(argument, ' ');

        if (message == NULL || message[1] == '\0') {
            dprintf(client_fd, "Error: Please provide a filename and text.\n.END.\n");
            return;
        }

        *message = '\0';
        message++;
        appendFile(argument, message, client_fd);
        dprintf(client_fd, ".END.\n");
        
    } else if (strncmp(command, "UPDATE", 6) == 0) {
        // Handle UPDATE command
        if (argument == NULL || strlen(argument) == 0) {
            dprintf(client_fd, "Error: No filename or update provided.\n.END.\n");
            return;
        }

        char *updateInfo = strchr(argument, ' ');

        if (updateInfo == NULL || updateInfo[1] == '\0') {
            dprintf(client_fd, "Error: Please provide a filename and text.\n.END.\n");
            return;
        }

        *updateInfo = '\0';
        updateInfo++;

        char *newMessage = strchr(updateInfo, '|');
        if (newMessage == NULL || newMessage[1] == '\0') {
            dprintf(client_fd, "Error: Please provide the new message after the delimiter.\n.END.\n");
            return;
        }
        *newMessage = '\0';
        newMessage++;

        if (strlen(updateInfo) == 0 || strlen(newMessage) == 0) {
            dprintf(client_fd, "Error: Please provide both the old and new messages.\n.END.\n");
            return;
        }
        
        updateFile(argument, updateInfo, newMessage, client_fd);
        dprintf(client_fd, ".END.\n");
        
    } else if (strncmp(command, "INFO", 4) == 0) {
        // Handle INFO command
        if (argument == NULL || strlen(argument) == 0) {
            dprintf(client_fd, "Error: No filename provided.\n.END.\n");
            return;
        }

        lastModified(argument, client_fd);
        dprintf(client_fd, ".END.\n");

    } else if (strncmp(command, "LIST", 4) == 0) {
        // Handle LIST command
        listFiles("", client_fd);
        dprintf(client_fd, ".END.\n");

    } else if (strncmp(command, "QUIT", 4) == 0) {
        // Handle QUIT command
        dprintf(client_fd, "Quitting server\n.END.\n");
        close(client_fd);
        return;
    } else {
        // Handle unknown command
        dprintf(client_fd, "Unknown command: %s\n.END.\n", command);
    }
}