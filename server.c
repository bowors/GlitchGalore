#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define DEFAULT_PORT 8080

void handle_get_request(int client_socket, const char* file_path) {
    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        char response[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nHello world!";
        write(client_socket, response, sizeof(response) - 1);
    } else {
        char response[1024];
        sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
        write(client_socket, response, strlen(response));

        char buffer[BUFFER_SIZE];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            write(client_socket, buffer, bytesRead);
        }

        fclose(file);
    }
}

void handle_post_request(const char* request_path) {
    char* id_str = strrchr(request_path, '/');
    if (id_str != NULL) {
        int id = atoi(id_str + 1);
        printf("Received POST request with ID: %d\n", id);
    }
}

void handle_client_request(int client_socket, const char* file_path) {
    char buffer[BUFFER_SIZE];
    read(client_socket, buffer, BUFFER_SIZE);

    // Parse the request method and path
    char method[10];
    char path[50];
    sscanf(buffer, "%s %s", method, path);

    if (strcmp(method, "GET") == 0) {
        handle_get_request(client_socket, file_path);
    } else if (strcmp(method, "POST") == 0 && strncmp(path, "/hook/", 6) == 0) {
        handle_post_request(path + 6);
        char response[] = "HTTP/1.1 200 OK\r\n\r\n";
        write(client_socket, response, sizeof(response) - 1);
    } else {
        char response[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
        write(client_socket, response, sizeof(response) - 1);
    }

    close(client_socket);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <file_path>\n", argv[0]);
        return 1;
    }

    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_length = sizeof(client_address);

    const char* file_path = argv[1];

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Failed to create socket");
        return 1;
    }

    // Set up server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(DEFAULT_PORT);

    // Bind the socket to the specified address and port
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 3) < 0) {
        perror("Listen failed");
        return 1;
    }

    printf("Server listening on port %d\n", DEFAULT_PORT);

    while (1) {
        // Accept a new connection
        client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_address_length);
        if (client_socket < 0) {
            perror("Accept failed");
            return 1;
        }

        // Handle the client request in a separate function
        handle_client_request(client_socket, file_path);
    }

    close(server_socket);
    return 0;
}
