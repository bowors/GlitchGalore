#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <microhttpd.h>

#define PORT 5150
#define PIPE_NAME "mypipe"

// Structure for a node in the list
struct node {
    char key[1024];
    const char *value;
    struct node *next;
};

// Default HTML content
const char *default_html_content = "<html><body><h1>Welcome to the Web Server!</h1></body></html>";

// Function to read file content
char *read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("fopen");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(file_size + 1);
    if (content == NULL) {
        perror("malloc");
        exit(1);
    }

    fread(content, 1, file_size, file);
    content[file_size] = '\0';

    fclose(file);

    return content;
}

// Function to insert a new key-value pair into the list
struct node *list_insert(struct node **head_ref, const char *key, const char *value) {
    struct node *new_node = malloc(sizeof(struct node));
    if (new_node == NULL) {
        perror("malloc");
        exit(1);
    }

    strncpy(new_node->key, key, sizeof(new_node->key));
    new_node->value = value;

    new_node->next = *head_ref;
    *head_ref = new_node;

    printf("Added key '%s' with value '%s' to list\n", key, value);

    return new_node;
}

// Function to find a value in the list given a key
const char *list_find(struct node *head, const char *key) {
    struct node *current;
    for (current = head; current != NULL; current = current->next) {
        if (strcmp(current->key, key) == 0) {
            return current->value;
        }
    }
    return NULL;
}

// Function to run process 2
void process2(struct node *list) {
    int fd;
    char buf[1024];
    ssize_t n;

    fd = open(PIPE_NAME, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    while (1) {
        n = read(fd, buf, sizeof(buf));
        if (n == -1) {
            perror("read");
            exit(1);
        } else if (n == 0) {
            break;
        } else {
            char *start = buf;
            char *end;
            while ((end = memchr(start, '\n', n - (start - buf))) != NULL) {
                printf("Process 2: received %.*s\n", (int)(end - start + 1), start);

                // Extract hook ID from the message
                int hook_id = atoi(start + 19);

                // Construct the key for the hook ID
                char key[1024];
                snprintf(key, sizeof(key), "request received from /hook/%d\n", hook_id);

                // Find the URL in the list using the key
                const char *url = list_find(list, key);
                if (url != NULL) {
                    printf("Process 2: found URL '%s' for message '%.*s'\n", url, (int)(end - start + 1), start);
                    // Add your logic here to handle the found URL (e.g., download the file)
                } else {
                    printf("Process 2: did not find URL for message '%.*s'\n", (int)(end - start + 1), start);
                }

                start = end + 1;
            }
        }
    }

    close(fd);
}

// Function to run process 3
void process3() {
    int ret;

    ret = system("ssh -T -o StrictHostKeyChecking=no "
                 "-R pingin-ngepot:80:localhost:5150 serveo.net || "
                 "echo \"Error: ssh command failed\"");
    if (ret != 0) {
        fprintf(stderr, "Failed to run ssh command\n");
    }
}

// Request handler function
int handle_request(void *cls,
                   struct MHD_Connection *connection,
                   const char *url,
                   const char *method,
                   const char *version,
                   const char *upload_data,
                   size_t *upload_data_size,
                   void **con_cls) {

    if (strcmp(method, "GET") == 0 && strcmp(url, "/") == 0) {
        // Handle GET request at '/'
        printf("Received GET request at %s\n", url);
        const char *content = (const char *)cls;
        struct MHD_Response *response = MHD_create_response_from_buffer(strlen(content), (void *)content, MHD_RESPMEM_PERSISTENT);
        int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }

    if (strcmp(method, "POST") == 0 && strncmp(url, "/hook/", 6) == 0) {
        // Handle POST request at '/hook/<int:hook_id>'
        printf("Received POST request at %s\n", url);

        // Extract the hook ID from the URL
        int hook_id = atoi(url + 6);

        // Find the corresponding URL in the list using the hook ID
        char key[1024];
        snprintf(key, sizeof(key), "request received from /hook/%d\n", hook_id);
        const char *hook_url = list_find(list, key);
        if (hook_url != NULL) {
            printf("Found URL '%s' for hook ID %d\n", hook_url, hook_id);
            // Add your logic here to handle the found URL (e.g., download the file)
        } else {
            printf("No URL found for hook ID %d\n", hook_id);
        }

        // Notify process 2 by writing to the named pipe
        int fd = open(PIPE_NAME, O_WRONLY);
        if (fd == -1) {
            perror("open");
            exit(1);
        }

        char message[1024];
        snprintf(message, sizeof(message), "request received from /hook/%d\n", hook_id);
        ssize_t n = write(fd, message, strlen(message));
        if (n == -1) {
            perror("write");
            exit(1);
        }

        close(fd);
    }

    return MHD_NO; // Return MHD_NO to indicate that the request was not handled
}

int main(int argc, char *argv[]) {
    struct MHD_Daemon *daemon;
    int ret;
    pid_t pid;
    char *html_content;
    struct node *list = NULL;

    if (argc < 2) {
        html_content = strdup(default_html_content);
    } else {
        html_content = read_file(argv[1]);
    }

    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL, &handle_request, (void *)html_content, MHD_OPTION_END);
    if (daemon == NULL) {
        fprintf(stderr, "Failed to start web server\n");
        exit(1);
    }

    // Create a named pipe
    ret = mkfifo(PIPE_NAME, 0666);
    if (ret == -1) {
        perror("mkfifo");
        exit(1);
    }

    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        // Child process (Process 2)
        process2(list);
    } else {
        // Parent process (Process 1)
        process3();
    }

    // Wait for the child process to terminate
    int status;
    wait(&status);

    // Cleanup
    MHD_stop_daemon(daemon);
    free(html_content);
    unlink(PIPE_NAME);

    return 0;
}
