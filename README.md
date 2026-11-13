First, compile and run the server using: 
    cd Server
    gcc -Wall -Wextra Server.c file_management_system.c deadlockDemo.c logging_system.c -o server -pthread
    ./server
You will see that the server starts on port number 5001 in the terminal.
Then, compile and run the client using:
    cd Client
    gcc Client.c -o client
    ./client
To connect to the server and get a menu with each of the options for the file operations.
