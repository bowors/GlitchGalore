#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_BUFFER_SIZE 4096

char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening file: %s\n", strerror(errno));
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(sizeof(char) * (file_size + 1));
    if (buffer == NULL) {
        fclose(file);
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    size_t read_size = fread(buffer, sizeof(char), file_size, file);
    if (read_size != file_size) {
        fclose(file);
        free(buffer);
        fprintf(stderr, "Error reading file: %s\n", strerror(errno));
        return NULL;
    }

    buffer[file_size] = '\0';

    fclose(file);
    return buffer;
}

int write_to_pipe(const char* pipe_name, const char* message) {
    int pipe_fd = open(pipe_name, O_WRONLY);
    if (pipe_fd == -1) {
        fprintf(stderr, "Error opening pipe: %s\n", strerror(errno));
        return -1;
    }

    ssize_t write_size = write(pipe_fd, message, strlen(message));
    if (write_size == -1) {
        close(pipe_fd);
        fprintf(stderr, "Error writing to pipe: %s\n", strerror(errno));
        return -1;
    }

    close(pipe_fd);
    return 0;
}

void handle_post_request(const char* pipe_name, const char* hook_id, const char* request_data) {
    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), "request received from /hook/%s\n", hook_id);
    if (write_to_pipe(pipe_name, message) == -1) {
        return;
    }

    // Print JSON output nicely
    json_error_t error;
    json_t* json = json_loads(request_data, 0, &error);
    if (json == NULL) {
        printf("%s\n", request_data);
    } else {
        char* formatted_json = json_dumps(json, JSON_INDENT(4));
        printf("%s\n", formatted_json);
        free(formatted_json);
        json_decref(json);
    }
}

int main() {
    const char* html_content = read_file("index.html");
    if (html_content == NULL) {
        return 1;
    }

    // Create the named pipe
    const char* pipe_name = "mypipe";
    if (mkfifo(pipe_name, 0666) == -1) {
        fprintf(stderr, "Error creating pipe: %s\n", strerror(errno));
        free(html_content);
        return 1;
    }

    // Start the server
    int port = 5150;
    printf("Server running on port %d\n", port);
    // Additional code for starting the server on the specified port goes here
    // ...

    free(html_content);
    return 0;
}
