/*
 * Line-based client. Reads commands from stdin, sends each to the server,
 * prints the reply until END_MARKER.
 *
 *   ./client                     localhost:5000
 *   ./client 10.0.0.5 5001       custom host/port
 *   ./client < commands.txt      scripted (no prompt when stdin isn't a tty)
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_HOST "127.0.0.1" //localhost
#define PORT         5001
#define MAX_LINE     1024 //maximum length of a line that can be read from stdin or sent to the server
#define END_MARKER   ".END."     //this is the marker that indicates the end of a server response

static void usage(void) {
    printf("Commands:\n"
           "  CREATE <file>\n"
           "  READ <file>\n" //reads the contents of a file
           "  APPEND <file> <text>\n" //appends text to the end of a file
           "  UPDATE <file> <old line>|<new line>\n" //updates a line in a file, replacing the old line with the new line
           "  INFO <file>\n" //displays information about a file, such as its size and last modified date
           "  LIST\n" //lists all files in the current directory
           "  QUIT\n");
}

int main(int argc, char *argv[]) {
    const char *host = (argc > 1) ? argv[1] : DEFAULT_HOST; //default host is localhost, take argument if provided
    int port         = (argc > 2) ? atoi(argv[2]) : PORT; //default port is 5000, take argument if provided

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { 
        perror("socket"); return 1;  //socket error
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET; //ipv4
    addr.sin_port   = htons(port);//convert port to network byte order
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "Bad address: %s\n", host);
        return 1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) { //-1 on error
        perror("connect");
        return 1;
    }
 // fdopen() associates a stream with the file descriptor fd, allowing us to use standard I/O functions like fgets() and fprintf() on the socket.
    FILE *in  = fdopen(fd, "r");
    FILE *out = fdopen(dup(fd), "w"); 
    int interactive = isatty(STDIN_FILENO);

    if (interactive) {
        printf("Connected to %s:%d\n", host, port);
        usage();
    }

    char line[MAX_LINE];
    for (;;) { //for ever loop
        if (interactive) { 
            printf("> "); fflush(stdout); 
        }
        if (fgets(line, sizeof line, stdin) == NULL) {
            fprintf(out, "QUIT\n");   //tell the server we are quitting
            fflush(out); //flush the output buffer to ensure the QUIT command is sent immediately
            break;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        if (strcmp(line, "HELP") == 0) { usage(); continue; }

        fprintf(out, "%s\n", line);
        fflush(out);

        /* Print the response until the terminator line */
        char resp[MAX_LINE];
        int gotEnd = 0;
        while (fgets(resp, sizeof resp, in) != NULL) { //while we geta response
            if (strcmp(resp, END_MARKER "\n") == 0) { 
                gotEnd = 1; break;
            }
            fputs(resp, stdout); //print the response to stdout
        }
        if (!gotEnd) {   //server closed connection before sending the end marker
            fprintf(stderr, "Server closed connection\n"); 
            break; 
        }
        if (strcmp(line, "QUIT") == 0){ //user wants to quit
         break;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}