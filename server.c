#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024
#define RESPONSE_200 "HTTP/1.1 200 OK\r\n\r\n"
#define RESPONSE_404 "HTTP/1.1 404 Not Found\r\n\r\n"

void handle_get_request(const char* filename, int client_socket) {
    FILE* file = fopen(filename, "r");
    if (file != NULL) {
        char response[BUFFER_SIZE];
        snprintf(response, BUFFER_SIZE, "%s", RESPONSE_200);
        send(client_socket, response, strlen(response), 0);
        
        char buffer[BUFFER_SIZE];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            send(client_socket, buffer, bytesRead, 0);
        }
        
        fclose(file);
    } else {
        char response[BUFFER_SIZE];
        snprintf(response, BUFFER_SIZE, "%sFile Not Found", RESPONSE_404);
        send(client_socket, response, strlen(response), 0);
    }
}

void handle_post_request(const char* id, int client_socket) {
    printf("Received POST request with ID: %s\n", id);
    char response[] = RESPONSE_200;
    send(client_socket, response, strlen(response), 0);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <html_file>\n", argv[0]);
        return 1;
    }
    
    const char* filename = argv[1];
    
    // TODO: Implement the HTTP server logic here, handling GET and POST requests
    
    // Example code to forward the HTTP server using SSH
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "ssh -T -o StrictHostKeyChecking=no -R pingin-ngepot:80:localhost:8080 serveo.net");
    system(command);
    
    return 0;
}
