#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <microhttpd.h>

#define PORT 8080
#define PIPE_NAME "named_pipe"

struct node {
    char key[1024];
    const char *value;
    struct node *next;
};

// Function to insert a new key-value pair into the list
struct node *list_insert(struct node **head_ref,
                         const char *key,
                         const char *value) {
    struct node *new_node;

    // Allocate a new node
    new_node = malloc(sizeof(struct node));
    if (new_node == NULL) {
        perror("malloc");
        exit(1);
    }

    // Set the key and value of the new node
    strncpy(new_node->key, key, sizeof(new_node->key));
    new_node->value = value;

    // Insert the new node at the head of the list
    new_node->next = *head_ref;
    *head_ref = new_node;

    printf("Added key '%s' with value '%s' to list\n", key, value);

    return new_node;
}

// Function to find a value in the list given a key
const char *list_find(struct node *head, const char *key) {
    struct node *current;

    // Iterate over the list and look for a node with the given key
    for (current = head; current != NULL; current = current->next) {
        if (strcmp(current->key, key) == 0) {
            return current->value;
        }
    }

    // Key not found in list
    return NULL;
}

int handle_request(void *cls, struct MHD_Connection *connection,
                   const char *url, const char *method, const char *version,
                   const char *upload_data, size_t *upload_data_size, void **ptr) {
    const char *content = "<html><body><h1>Hello, World!</h1></body></html>";
    struct MHD_Response *response;

    if (strcmp(url, "/") == 0) {
        response = MHD_create_response_from_buffer(strlen(content), (void *)content, MHD_RESPMEM_PERSISTENT);
        int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    } else if (strstr(url, "/hook/") == url) {
        int hook_id = atoi(url + strlen("/hook/"));
        char key[1024];
        snprintf(key, sizeof(key), "request received from /hook/%d\n", hook_id);
        const char *hook_url = list_find((struct node *)cls, key);
        if (hook_url != NULL) {
            printf("Process 2: found URL '%s' for key '%s'\n", hook_url, key);
            // Add your logic here to handle the found URL (e.g., download file)
        } else {
            printf("Process 2: did not find URL for key '%s'\n", key);
        }
    }

    response = MHD_create_response_from_buffer(strlen(content), (void *)content, MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

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
                *end = '\0';
                printf("Process 2: received '%s'\n", start);

                // Add your logic here to handle the received message
                // You can use the list_find function to find the URL associated with the message

                start = end + 1;
            }
        }
    }

    close(fd);
}

void process3() {
    int ret;

    ret = system("ssh -T -o StrictHostKeyChecking=no "
                 "-R pingin-ngepot:80:localhost:8080 serveo.net || "
                 "echo \"Error: ssh command failed\"");
    if (ret != 0) {
        fprintf(stderr, "Failed to run ssh command\n");
    }
}

int main(int argc, char *argv[]) {
    struct MHD_Daemon *daemon;
    struct node *list = NULL;
    char *html_content;

    if (argc < 2) {
        html_content = strdup("<html><body><h1>Hello, World!</h1></body></html>");
    } else {
        html_content = read_file(argv[1]);
    }

    for (int i = 2; i < argc; i++) {
        char key[1024];
        snprintf(key, sizeof(key), "request received from /hook/%d\n", i - 1);
        list_insert(&list, key, argv[i]);
    }

    int ret = mkfifo(PIPE_NAME, 0666);
    if (ret != 0) {
        perror("mkfifo");
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        process2(list);
    } else {
        daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
                                  PORT,
                                  NULL, NULL, &handle_request, (void *)list,
                                  MHD_OPTION_END);
        if (daemon == NULL) {
            fprintf(stderr, "Failed to start server\n");
            return 1;
        }

        process3();

        getchar();

        MHD_stop_daemon(daemon);

        wait(NULL);
    }

    free(html_content);

    return 0;
}
