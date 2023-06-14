#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <curl/curl.h>
#include <microhttpd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define PORT 8080
#define SOCKET_PATH "/tmp/example.sock"

// Default HTML content to serve
const char *default_html_content =
    "<html><body>"
    "<h1>Example Web Server</h1>"
    "<p>This is an example web server.</p>"
    "</body></html>";

// Function to download a file from a URL using libcurl
void download_file(const char *url) {
    CURL *curl;
    CURLcode res;

    // Initialize libcurl
    curl = curl_easy_init();
    if (curl) {
        // Set options for the curl easy handle
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

        // Perform the download
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "Failed to download file from %s: %s\n",
                    url, curl_easy_strerror(res));
        } else {
            printf("Successfully downloaded file from %s\n", url);
        }

        // Clean up libcurl resources
        curl_easy_cleanup(curl);
    } else {
        fprintf(stderr, "Failed to initialize libcurl\n");
    }
}

// Function to handle data received by libcurl
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, stdout);
    return written;
}

// Linked list node structure
struct node {
    char *key;
    char *value;
    struct node *next;
};

// Function to insert a key-value pair into a linked list
void list_insert(struct node **list, const char *key, const char *value) {
    struct node *new_node = malloc(sizeof(struct node));
    new_node->key = strdup(key);
    new_node->value = strdup(value);
    new_node->next = *list;
    *list = new_node;

    printf("Added key '%s' with value '%s' to list\n", key, value);
}

// Function to find a value in a linked list by key
const char *list_find(struct node *list, const char *key) {
    while (list != NULL) {
        if (strcmp(list->key, key) == 0) {
            return list->value;
        }
        list = list->next;
    }
    return NULL;
}

// Function to handle HTTP requests
static int handle_request(void *cls, struct MHD_Connection *connection,
                          const char *url, const char *method,
                          const char *version, const char *upload_data,
                          size_t *upload_data_size, void **con_cls) {
    struct MHD_Response *response;
    int ret;

    printf("Received %s request at %s\n", method, url);

    // Check if this is a GET request for the '/' URL
    if (strcmp(method, "GET") == 0 && strcmp(url, "/") == 0) {
        // Serve HTML content for GET request at '/'
        const char *html_content = cls;
        response = MHD_create_response_from_buffer(strlen(html_content),
                                                   (void *) html_content,
                                                   MHD_RESPMEM_PERSISTENT);
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    } else if (strcmp(method, "POST") == 0 && strncmp(url, "/hook/", 6) == 0) {
        // Handle POST request at '/hook/<int:hook_id>'
        printf("Handling POST request at %s\n", url);

        // Notify process 2 by sending a message over the Unix domain socket
        int fd;
        ssize_t n;
        struct sockaddr_un addr;
        char msg[1024];

        // Format a message to send to process 2
        snprintf(msg, sizeof(msg), "request received from %s\n", url);

        // Create a Unix domain socket
        fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (fd == -1) {
            perror("socket");
            exit(1);
        }

        // Set the address of the Unix domain socket
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

        // Send the message over the Unix domain socket
        n = sendto(fd, msg, strlen(msg), 0, (struct sockaddr *) &addr, sizeof(addr));
        if (n == -1) {
            perror("sendto");
            exit(1);
        } else {
            printf("Sent message '%s' over Unix domain socket\n", msg);
        }

        // Close the Unix domain socket
        close(fd);

        // Send an empty response with status code 200 OK
        response = MHD_create_response_from_buffer(0,NULL,MHD_RESPMEM_PERSISTENT );
         ret=MHD_queue_response(connection,MHD_HTTP_OK,response );
         MHD_destroy_response(response );
         return ret;
     } else {
         printf("Ignoring %s request at %s\n", method, url);
     }
}

// Function to run process 2
void process2(struct node *list) {
    int fd;
    ssize_t n;
    struct sockaddr_un addr;
    socklen_t addrlen;
    char buf[1024];

    // Create a Unix domain socket
    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        exit(1);
    }

    // Set the address of the Unix domain socket
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Bind the Unix domain socket to the specified address
    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(1);
    }

    while (1) {
        // Receive data from the Unix domain socket
        addrlen = sizeof(addr);
        n = recvfrom(fd, buf, sizeof(buf), 0,
                     (struct sockaddr *) &addr,&addrlen );
         if(n==-1){
             perror("recvfrom");
             exit(1);
         }else{
             // Process one message
             printf("Process 2: received %.*s\n",(int)n,buf );

             // Add logic here to handle messages from process 1
             const char *url=list_find(list,buf );
             if(url!=NULL){
                 printf("Process 2: found URL '%s' for message '%.*s'\n",url,(int)n,buf );
                 download_file(url);
             }else{
                 printf("Process 2: did not find URL for message '%.*s'\n",(int)n,buf );
             }
         }
    }

    // Close the Unix domain socket
    close(fd);
}

// Function to read an entire file into a string
char *read_file(const char *filename) {
    FILE *fp;
    long len;
    char *buf;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = malloc(len + 1);
    if (buf == NULL) {
        perror("malloc");
        exit(1);
    }

    fread(buf, 1, len, fp);
    buf[len] = '\0';

    fclose(fp);

    return buf;
}

int main(int argc, char **argv) {
    struct MHD_Daemon *daemon;
    struct node *list = NULL;
    pid_t pid;
    int status;

    // Check command line arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <html-file>\n", argv[0]);
        exit(1);
    }

    // Read HTML content from file
    const char *html_content = read_file(argv[1]);

    // Create a linked list to store key-value pairs
    list_insert(&list, "request received from /hook/1\n", "https://example.com");
    list_insert(&list, "request received from /hook/2\n", "https://google.com");
    list_insert(&list, "request received from /hook/3\n", "https://bing.com");

    // Fork a child process to run process 2
    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        // Child process: run process 2
        process2(list);
        exit(0);
    } else {
        // Parent process: run process 1 (web server)
        daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
                                  &handle_request, html_content,
                                  MHD_OPTION_END);
        if (daemon == NULL) {
            fprintf(stderr, "Failed to start web server\n");
            exit(1);
        }

        // Wait for child process to exit
        wait(&status);

        // Stop the web server
        MHD_stop_daemon(daemon);
    }

    return 0;
}
