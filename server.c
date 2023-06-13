#include "mongoose.h"
#include <stdio.h>

static const char *s_http_port = "5150";
static struct mg_serve_http_opts s_http_server_opts;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <named_pipe>\n", argv[0]);
    exit(1);
  }
  char *named_pipe = argv[1];

  static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *hm = (struct http_message *) ev_data;
    switch (ev) {
      case MG_EV_HTTP_REQUEST:
        if (mg_vcmp(&hm->uri, "/") == 0) {
          mg_printf(nc, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n%s", "OK");
        } else if (mg_vcmp(&hm->uri, "/hook") == 0) {
          char hook_id[100];
          mg_get_http_var(&hm->body, "hook_id", hook_id, sizeof(hook_id));
          printf("request received from /hook/%s\n", hook_id);
          FILE *f = fopen(named_pipe, "w");
          if (f != NULL) {
            fprintf(f, "request received from /hook/%s\n", hook_id);
            fclose(f);
          }
          char buf[100];
          snprintf(buf, sizeof(buf), "%.*s", (int) hm->body.len, hm->body.p);
          printf("%s\n", buf);
          mg_printf(nc, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n%s", "Hello world");
        } else {
          mg_serve_http(nc, hm, s_http_server_opts);
        }
        break;
      default:
        break;
    }
  }

  struct mg_mgr mgr;
  struct mg_connection *nc;

  mg_mgr_init(&mgr, NULL);
  nc = mg_bind(&mgr, s_http_port, ev_handler);

  mg_set_protocol_http_websocket(nc);
  s_http_server_opts.document_root = ".";

  printf("Starting web server on port %s\n", s_http_port);
  for (;;) {
    mg_mgr_poll(&mgr, 1000);
  }
  mg_mgr_free(&mgr);

  return 0;
}
