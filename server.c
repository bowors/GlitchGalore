#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define DEFAULT_RESPONSE "Hello world!"

void serve_get_request(const char* file_path, int client_socket) {
    FILE* file;
    char response[BUFFER_SIZE];

    if (file_path == NULL) {
        file = fopen("index.html", "r");
        if (file == NULL) {
            write(client_socket, DEFAULT_RESPONSE, strlen(DEFAULT_RESPONSE));
            return;
        }
    } else {
        file = fopen(file_path, "r");
        if (file == NULL) {
            write(client_socket, DEFAULT_RESPONSE, strlen(DEFAULT_RESPONSE));
            return;
        }
    }

    sprintf(response, "HTTP/1.1 200 OK\r\n\r\n");
    write(client_socket, response, strlen(response));

    while (fgets(response, BUFFER_SIZE, file) != NULL) {
        write(client_socket, response, strlen(response));
    }

    fclose(file);
}

void serve_post_request(const char* id, int client_socket) {
    char response[BUFFER_SIZE];
    sprintf(response, "HTTP/1.1 200 OK\r\n\r\n");
    write(client_socket, response, strlen(response));

    printf("Received POST request with id: %s\n", id);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Please provide an HTML file as an argument.\n");
        return 1;
    }

    // Extract the file name from the argument
    const char* file_name = argv[1];

    // Set up a simple HTTP server
    int server_socket, client_socket;
    char request[BUFFER_SIZE];
    char method[BUFFER_SIZE];
    char url[BUFFER_SIZE];
    char protocol[BUFFER_SIZE];

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_address, client_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Socket binding failed");
        return 1;
    }

    if (listen(server_socket, 5) == -1) {
        perror("Socket listening failed");
        return 1;
    }

    printf("Server listening on port 8080...\n");

    while (1) {
        socklen_t client_address_length = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_address_length);
        if (client_socket == -1) {
            perror("Failed to accept client connection");
            return 1;
        }

        read(client_socket, request, BUFFER_SIZE);

        sscanf(request, "%s %s %s", method, url, protocol);

        if (strcmp(method, "GET") == 0 && strcmp(url, "/") == 0) {
            serve_get_request(file_name, client_socket);
        } else if (strcmp(method, "POST") == 0) {
            // Extract the id from the URL
            char* id_start = strstr(url, "/hook/");
            if (id_start != NULL) {
                char id[BUFFER_SIZE];
                sscanf(id_start, "/hook/%s", id);
                serve_post_request(id, client_socket);
            } else {
                char response[BUFFER_SIZE];
                sprintf(response, "HTTP/1.1 404 Not Found\r\n\r\n");
                write(client_socket, response, strlen(response));
            }
        } else {
            char response[BUFFER_SIZE];
            sprintf(response, "HTTP/1.1 404 Not Found\r\n\r\n");
            write(client_socket, response, strlen(response));
        }

        close(client_socket);
    }

    close(server_socket);

    return 0;
}
