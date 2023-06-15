#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

// Function to handle GET requests
void handle_get(int client_socket) {
    FILE *file = fopen("index.html", "r");
    if (file == NULL) {
        printf("Error opening file.\n");
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (write(client_socket, buffer, bytesRead) < 0) {
            printf("Error writing to socket.\n");
            exit(1);
        }
        memset(buffer, 0, sizeof(buffer));
    }

    fclose(file);
}

// Function to handle POST requests
void handle_post(int client_socket, int hook_id, char* request_data) {
    FILE *pipe = fopen("mypipe", "w");
    if (pipe == NULL) {
        printf("Error opening pipe.\n");
        exit(1);
    }

    fprintf(pipe, "request received from /hook/%d\n", hook_id);
    fclose(pipe);

    // Print request_data or parse it using a JSON library

    char response[] = "Hello world";
    if (write(client_socket, response, strlen(response)) < 0) {
        printf("Error writing to socket.\n");
        exit(1);
    }
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_length = sizeof(client_address);

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        printf("Error creating socket.\n");
        exit(1);
    }

    // Set server address
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(5150);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // Bind the server socket to the specified address and port
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printf("Error binding socket.\n");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) < 0) {
        printf("Error listening on socket.\n");
        exit(1);
    }

    while (1) {
        // Accept a client connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
        if (client_socket < 0) {
            printf("Error accepting connection.\n");
            exit(1);
        }

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));

        // Read the client request
        ssize_t bytesRead = read(client_socket, buffer, sizeof(buffer) - 1);
        if (bytesRead < 0) {
            printf("Error reading from socket.\n");
            exit(1);
        }

        // Parse the request
        char method[10];
        int hook_id;
        sscanf(buffer, "%9s /hook/%d", method, &hook_id);

        if (strcmp(method, "GET") == 0) {
            handle_get(client_socket);
        } else if (strcmp(method, "POST") == 0) {
            char *dataStart = strstr(buffer, "\r\n\r\n") + 4;
            handle_post(client_socket, hook_id, dataStart);
        }

        // Close the client connection
        close(client_socket);
    }

    // Close the server socket
    close(server_socket);

    return 0;
}
