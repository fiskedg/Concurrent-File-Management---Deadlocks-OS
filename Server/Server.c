#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "file_management_system.h"

#define QUEUE_SIZE 10 
#define NUM_WORKERS 6
#define PORT 5000
#define BUFFER_SIZE 4096

//To compile run gcc -Wall -Wextra Server.c file_management_system.c -o server -pthread

void process_command(char *command, int client_fd);

typedef struct {
    char songName[50];
    char filePath[100];
} Song;

typedef struct {
    int client_fd;
} task_t;

typedef struct {
    task_t queue[QUEUE_SIZE];
    int front;
    int rear;
    int count;
    int shutdown;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    pthread_t workers[NUM_WORKERS];
} thread_pool_t;




//good ol client , each worker thread 1 per job
void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE]; // Local thread-safe buffer

    int n;
    
    while ((n = read(client_fd, buffer, BUFFER_SIZE-1)) > 0) {
        
        buffer[n] = '\0';
        buffer[strcspn(buffer, "\r\n")] = 0; // Strip newlines
        printf("[Worker %lu] Requested: %s\n", (unsigned long)pthread_self(), buffer);
        process_command(buffer, client_fd);
    }
    if (n == -1) {
        perror("read");
    }
    printf("[Worker %lu] Client Disconnected. \n",
    (unsigned long)pthread_self());
    close(client_fd);
}


//this is the worker function, lowlevel worker logic
void *worker(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;

    while (1) {
        task_t task;

        pthread_mutex_lock(&pool->mutex); //lock mutex and wait for job
        while (pool->count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->not_empty, &pool->mutex); 
        }

        if (pool->count == 0 && pool->shutdown) { //shutdown conditon 
            pthread_mutex_unlock(&pool->mutex);
            break;
        } //unlock and break out, returning null

        task = pool->queue[pool->front];
        pool->front = (pool->front + 1) % QUEUE_SIZE; //queue is circular, so we wrap around
        pool->count--; //decrament count of jobs in queue

        pthread_cond_signal(&pool->not_full); 
        pthread_mutex_unlock(&pool->mutex); //unlock the 

        handle_client(task.client_fd); //execute the job.
    }
    return NULL;
}

//initalizes the thread pool and creates the worker threads
void pool_init(thread_pool_t *pool) {
    pool->front = 0;
    pool->rear = 0;
    pool->count = 0;
    pool->shutdown = 0;
    //initalize pool values

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->not_empty, NULL);
    pthread_cond_init(&pool->not_full, NULL);

    //create workers
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&pool->workers[i], NULL, worker, pool);
    }
}


//thread safe enqueue
//we use mutex to act as a syncronizer 
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
    //create and initalize the thread pool
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

    server_addr.sin_family = AF_INET; // Use IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    server_addr.sin_port = htons(PORT); //assign port to listen on

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
    return 0;
}

void process_command(char *command, int client_fd) {

    char *argument = strchr(command, ' ');

    if (argument != NULL) {
        *argument = '\0';
        argument++;
    }

    if (strncmp(command, "CREATE", 6) == 0) {
        // Handle CREATE command
        if (argument == NULL || strlen(argument) == 0) {
            dprintf(client_fd, "Error: No file name provided.\n.END.\n");
            return;
        }
        FILE *newFile = createOrOpenFile(NULL, argument);
        if (newFile == NULL) {
            dprintf(client_fd, "Error: Failed to create or open the file.\n.END.\n");
            return;
        } else {
            fclose(newFile);
            dprintf(client_fd, "File created successfully: %s\n.END.\n", argument);
        }
       
    } else if (strncmp(command, "READ", 4) == 0) {
        // Handle READ command
        dprintf(client_fd, "Reading file: %s\n.END.\n", argument);
    } else if (strncmp(command, "APPEND", 6) == 0) {
        // Handle APPEND command
        dprintf(client_fd, "Appending to file: %s\n.END.\n", argument);
    } else if (strncmp(command, "UPDATE", 6) == 0) {
        // Handle UPDATE command
        dprintf(client_fd, "Updating file: %s\n.END.\n", argument);
    } else if (strncmp(command, "INFO", 4) == 0) {
        // Handle INFO command
        dprintf(client_fd, "Getting info for file: %s\n.END.\n", argument);
    } else if (strncmp(command, "LIST", 4) == 0) {
        // Handle LIST command
        dprintf(client_fd, "Listing all files\n.END.\n");
    } else if (strncmp(command, "QUIT", 4) == 0) {
        // Handle QUIT command
        dprintf(client_fd, "Quitting server\n.END.\n");
    } else {
        // Handle unknown command
        dprintf(client_fd, "Unknown command: %s\n.END.\n", command);
    }
}