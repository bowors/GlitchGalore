#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <sys/wait.h>

#define PORT 5150
#define PIPE_NAME "mypipe"

const char *default_html_content = "<html><body><h1>Hello World</h1></body></html>";

char *read_file(const char *filename) {
    FILE *file;
    long length;
    char *buffer;

    file = fopen(filename, "rb");
    if (file == NULL) {
        perror("fopen");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = malloc(length + 1);
    if (buffer == NULL) {
        perror("malloc");
        exit(1);
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';

    fclose(file);

    return buffer;
}

static int handle_request(void *cls, struct MHD_Connection *connection,
                          const char *url, const char *method,
                          const char *version, const char *upload_data,
                          size_t *upload_data_size, void **con_cls) {
    struct MHD_Response *response;
    int ret;

    if (strcmp(method, "GET") == 0 && strcmp(url, "/") == 0) {
        // Handle GET request at '/'
        const char *html_content = cls;
        response = MHD_create_response_from_buffer(strlen(html_content),
                                                   (void *) html_content,
                                                   MHD_RESPMEM_PERSISTENT);
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    } else if (strcmp(method, "POST") == 0 && strncmp(url, "/hook/", 6) == 0) {
        // Handle POST request at '/hook/<int:hook_id>'
        printf("Received POST request at %s\n", url);

        // Notify process 2 by writing to the named pipe
        int fd;
        ssize_t n;
        char msg[1024];

        snprintf(msg, sizeof(msg), "request received from %s\n", url);

        fd = open(PIPE_NAME, O_WRONLY);
        if (fd == -1) {
            perror("open");
            exit(1);
        }

        n = write(fd, msg, strlen(msg));
        if (n == -1) {
            perror("write");
            exit(1);
        }

        close(fd);

        response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    } else {
        // Return 404 for all other requests
        response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
        ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);
        return ret;
    }
}

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp) {
    // Write downloaded data to stdout
    fwrite(buffer, size, nmemb, stdout);

    return size * nmemb;
}

void download_file(const char *url) {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL,url);
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION ,write_data);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr,"Failed to download file from %s: %s\n",
                    url,curl_easy_strerror(res));
        }

       curl_easy_cleanup(curl); 
   }
}

struct node {
    char key[1024];
    const char *value;
    struct node *next;
};

struct node *list_insert(struct node **head_ref,
                         const char *key,
                         const char *value) {
    struct node *new_node;

    new_node = malloc(sizeof(struct node));
    if (new_node == NULL) {
        perror("malloc");
        exit(1);
    }

    strncpy(new_node->key, key, sizeof(new_node->key));
    new_node->value = value;
    new_node->next = *head_ref;
    *head_ref = new_node;

    return new_node;
}

const char *list_find(struct node *head, const char *key) {
    struct node *current;

    for (current = head; current != NULL; current = current->next) {
        if (strcmp(current->key, key) == 0) {
            return current->value;
        }
    }

    return NULL;
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
        n = read(fd,buf, sizeof(buf));
        if (n == -1) {
            perror("read");
            exit(1);
        } else if (n == 0) {
            break;
        } else {
            printf("Process 2: received %.*s", (int) n, buf);

            // Add logic here to handle messages from process 1
            const char *url = list_find(list, buf);
            if (url != NULL) {
                download_file(url);
            }
        }
    }

    close(fd);
}

void process3() {
    int ret;

    ret = system("ssh -T -o StrictHostKeyChecking=no "
                 "-R pingin-ngepot:80:localhost:5150 serveo.net || "
                 "echo \"Error: ssh command failed\"");
    if (ret != 0) {
        fprintf(stderr, "Failed to run ssh command\n");
    }
}

int main(int argc, char *argv[]) {
    struct MHD_Daemon *daemon;
    int ret;
    pid_t pid;
    char *html_content;
    struct node *list = NULL;

    if (argc < 2) {
        // Use default HTML content
        html_content = strdup(default_html_content);
    } else {
        // Read HTML content from file
        html_content = read_file(argv[1]);
    }

    // Add URLs to list
    for (int i = 2; i < argc; i++) {
        char key[1024];
        snprintf(key, sizeof(key), "request received from /hook/%d\n", i - 1);
        list_insert(&list, key, argv[i]);
    }

    // Create named pipe
    ret = mkfifo(PIPE_NAME, 0666);
    if (ret != 0) {
        perror("mkfifo");
        exit(1);
    }

    // Create child process for process 2
    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        // Child process: run process 2
        process2(list);
    } else {
        // Parent process: run web server (process 1)
        daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
                                  PORT,
                                  NULL,
                                  NULL,
                                  &handle_request,
                                  html_content,
                                  MHD_OPTION_END);
        if (daemon == NULL) {
            fprintf(stderr, "Failed to start server\n");
            return 1;
        }

        // Start process 3
        process3();

        getchar();

        MHD_stop_daemon(daemon);

         // Wait for child process to terminate
         wait(NULL);
     }

     free(html_content);

     return 0;
}
