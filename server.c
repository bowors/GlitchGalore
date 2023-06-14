#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <sys/wait.h>

// Define constants for the port number and named pipe
#define PORT 5150
#define PIPE_NAME "mypipe"

// Define default HTML content to serve
const char *default_html_content = "<html><body><h1>Hello World</h1></body></html>";

// Function to read the contents of a file into a buffer
char *read_file(const char *filename) {
    FILE *file;
    long length;
    char *buffer;

    // Open the file for reading
    file = fopen(filename, "rb");
    if (file == NULL) {
        perror("fopen");
        exit(1);
    }

    // Determine the length of the file
    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate a buffer to hold the contents of the file
    buffer = malloc(length + 1);
    if (buffer == NULL) {
        perror("malloc");
        exit(1);
    }

    // Read the contents of the file into the buffer
    fread(buffer, 1, length, file);
    buffer[length] = '\0';

    // Close the file and return the buffer
    fclose(file);

    return buffer;
}

// Function to handle HTTP requests
static int handle_request(void *cls, struct MHD_Connection *connection,
                          const char *url, const char *method,
                          const char *version, const char *upload_data,
                          size_t *upload_data_size, void **con_cls) {
    struct MHD_Response *response;
    int ret;

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
        printf("Received POST request at %s\n", url);

        // Notify process 2 by writing to the named pipe
        int fd;
        ssize_t n;
        char msg[1024];

        // Format a message to send to process 2
        snprintf(msg, sizeof(msg), "request received from %s\n", url);

        // Open the named pipe for writing
        fd = open(PIPE_NAME, O_WRONLY);
        if (fd == -1) {
            perror("open");
            exit(1);
        }

        // Write the message to the named pipe
        n = write(fd, msg, strlen(msg));
        if (n == -1) {
            perror("write");
            exit(1);
        }

        // Close the named pipe
        close(fd);

        // Send an empty response with status code 200 OK
        response = MHD_create_response_from_buffer(0,NULL,MHD_RESPMEM_PERSISTENT );
         ret=MHD_queue_response(connection,MHD_HTTP_OK,response );
         MHD_destroy_response(response );
         return ret;
     }
}

// Function to write downloaded data to stdout
size_t write_data(void *buffer,size_t size,size_t nmemb,void *userp){
   fwrite(buffer,size,nmemb ,stdout);

   return size*nmemb ;
}

// Function to download a file from a URL using libcurl
void download_file(const char *url){
   CURL *curl;
   CURLcode res;

   // Initialize libcurl
   curl=curl_easy_init();
   if(curl){
      // Set options for the curl easy handle
      curl_easy_setopt(curl,CURLOPT_URL,url );
      curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION ,write_data);

      // Perform the download
      res=curl_easy_perform(curl);
      if(res!=CURLE_OK){
         fprintf(stderr,"Failed to download file from %s: %s\n",
                 url,curl_easy_strerror(res));
      }

      // Clean up libcurl resources
      curl_easy_cleanup(curl); 
   }
}

// Define a node structure for a linked list of key-value pairs
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

// Function to run process 2
void process2(struct node *list) {
    int fd;
    char buf[1024];
    ssize_t n;

    // Open the named pipe for reading
    fd = open(PIPE_NAME, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    while (1) {
        // Read data from the named pipe
        n = read(fd, buf, sizeof(buf));
        if (n == -1) {
            perror("read");
            exit(1);
        } else if (n == 0) {
            break;
        } else {
            // Split the buffer into separate messages
            char *start = buf;
            char *end;
            while ((end = memchr(start, '\n', n - (start - buf))) != NULL) {
                // Process one message
                printf("Process 2: received %.*s\n", (int)(end - start + 1), start);

                // Add logic here to handle messages from process 1
                const char *url = list_find(list, start);
                if (url != NULL) {
                    printf("Process 2: found URL '%s' for message '%.*s'\n", url, (int)(end - start + 1), start);
                    download_file(url);
                } else {
                    printf("Process 2: did not find URL for message '%.*s'\n", (int)(end - start + 1), start);
                }

                start = end + 1;
            }
        }
    }

    // Close the named pipe
    close(fd);
}


// Function to run process 3
void process3() {
    int ret;

    // Run an ssh command to forward a remote port to the local web server
    ret = system("ssh -T -o StrictHostKeyChecking=no "
                 "-R pingin-ngepot:80:localhost:5150 serveo.net || "
                 "echo \"Error: ssh command failed\"");
    if (ret != 0) {
        fprintf(stderr, "Failed to run ssh command\n");
    }
}

// Main function
int main(int argc, char *argv[]) {
    struct MHD_Daemon *daemon;
    int ret;
    pid_t pid;
    char *html_content;
    struct node *list = NULL;

    // Check if an HTML file was specified on the command line
    if (argc < 2) {
        // Use default HTML content
        html_content = strdup(default_html_content);
    } else {
        // Read HTML content from file
        html_content = read_file(argv[1]);
    }

    // Add URLs to list from command line arguments
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
