#include <evhtp.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void home_cb(evhtp_request_t * req, void * arg) {
    const char * html_content = (const char *)arg;
    evbuffer_add_printf(req->buffer_out, "%s", html_content);
    evhtp_send_reply(req, EVHTP_RES_OK);
}

void hook_cb(evhtp_request_t * req, void * arg) {
    const char * hook_id = (const char *)arg;
    FILE * f = fopen("mypipe", "w");
    if (f != NULL) {
        fprintf(f, "request received from /hook/%s\n", hook_id);
        fclose(f);
    }

    struct evbuffer * input_buffer = evhtp_request_get_input_buffer(req);
    size_t input_len = evbuffer_get_length(input_buffer);
    unsigned char * input_data = evbuffer_pullup(input_buffer, input_len);

    // Format JSON output nicely and print it
    json_error_t error;
    json_t * data = json_loadb((const char *)input_data, input_len, 0, &error);
    if (data != NULL) {
        char * json_str = json_dumps(data, JSON_INDENT(4));
        printf("%s\n", json_str);
        free(json_str);
        json_decref(data);
    } else {
        printf("%.*s\n", (int)input_len, input_data);
    }

    evbuffer_add_printf(req->buffer_out, "Hello world");
    evhtp_send_reply(req, EVHTP_RES_OK);
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: missing html_content argument\n");
        return 1;
    }
    const char * html_content = argv[1];

    evbase_t * evbase = event_base_new();
    evhtp_t  * htp = evhtp_new(evbase, NULL);

    evhtp_set_cb(htp, "/", home_cb, (void *)html_content);

    for (int i = 1; i <= 3; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/hook/%d", i);

        char * hook_id = malloc(16);
        snprintf(hook_id, 16, "%d", i);

        evhtp_set_cb(htp, path, hook_cb, hook_id);
    }

    evhtp_bind_socket(htp, "0.0.0.0", 5150, 1024);

    event_base_loop(evbase, 0);

    return 0;
}
