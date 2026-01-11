/**
 * Simple Markdown Blog Server (C)
 * Uses cmark-gfm for Markdown rendering
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include <cmark-gfm.h>
#include <cmark-gfm-core-extensions.h>

#define BUFFER_SIZE 8192
#define MAX_PATH 256

typedef struct
{
    int client_fd;
    char blog_dir[MAX_PATH];
} client_info_t;

/* ================= HTML TEMPLATE ================= */

const char *HTML_HEADER =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "  <meta charset=\"UTF-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
    "  <title>%s</title>\n"
    "  <style>\n"
    "    body { max-width: 800px; margin: 40px auto; padding: 0 20px;"
    "           font-family: -apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;"
    "           line-height: 1.6; color: #333; }\n"
    "    h1 { border-bottom: 2px solid #3498db; padding-bottom: 10px; }\n"
    "    code { background: #f4f4f4; color: #333; padding: 2px 6px; border-radius: 3px; }\n"
    "    pre { background: #2d2d2d; color: #f8f8f2; padding: 15px; border-radius: 5px; overflow-x: auto; }\n"
    "    pre code { background: none; color: inherit; padding: 0; }\n"
    "    a { color: #3498db; text-decoration: none; }\n"
    "    nav { margin-bottom: 20px; }\n"
    "    .post-list { list-style: none; padding: 0; }\n"
    "    .post-list li { margin: 15px 0; padding: 15px;"
    "                    border-left: 3px solid #3498db; background: #f8f9fa; }\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">← Home</a></nav>\n";

const char *HTML_FOOTER =
    "</body>\n"
    "</html>\n";

/* ================= SAFE APPEND MACRO ================= */

#define APPEND(fmt, ...)                                    \
    do                                                      \
    {                                                       \
        int n = snprintf(p, remaining, fmt, ##__VA_ARGS__); \
        if (n < 0 || n >= (int)remaining)                   \
            goto overflow;                                  \
        p += n;                                             \
        remaining -= n;                                     \
    } while (0)

/* ================= UTILS ================= */

char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buf = malloc(size + 1);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, size, f);
    buf[size] = 0;
    fclose(f);
    return buf;
}

void get_title_from_markdown(const char *md, char *title, size_t sz)
{
    const char *p = md;
    while (*p && isspace(*p))
        p++;
    while (*p == '#')
        p++;
    while (*p && isspace(*p))
        p++;

    size_t i = 0;
    while (*p && *p != '\n' && i < sz - 1)
        title[i++] = *p++;

    title[i] = 0;
    if (!title[0])
        strncpy(title, "Untitled", sz);
}

/* ================= MARKDOWN ================= */

char *markdown_to_html(const char *markdown)
{
    // Enable GitHub Flavored Markdown extensions
    cmark_gfm_core_extensions_ensure_registered();

    cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT);

    // Enable table extension
    cmark_syntax_extension *table_ext = cmark_find_syntax_extension("table");
    if (table_ext)
    {
        cmark_parser_attach_syntax_extension(parser, table_ext);
    }

    cmark_parser_feed(parser, markdown, strlen(markdown));
    cmark_node *doc = cmark_parser_finish(parser);

    char *html = cmark_render_html(doc, CMARK_OPT_DEFAULT, NULL);

    cmark_node_free(doc);
    cmark_parser_free(parser);

    return html;
}

/* ================= HTTP ================= */

void send_response(int fd, const char *status,
                   const char *type, const char *body)
{
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s; charset=utf-8\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n",
             status, type, strlen(body));
    write(fd, header, strlen(header));
    write(fd, body, strlen(body));
}

/* ================= ROUTES ================= */

void serve_index(int fd, const char *blog_dir)
{
    DIR *dir = opendir(blog_dir);
    if (!dir)
    {
        send_response(fd, "500 Internal Server Error",
                      "text/html", "<h1>Cannot open blog dir</h1>");
        return;
    }

    char response[BUFFER_SIZE * 4];
    char *p = response;
    size_t remaining = sizeof(response);

    APPEND(HTML_HEADER, "My Blog");
    APPEND("<h1>My Blog</h1><ul class=\"post-list\">");

    struct dirent *e;
    while ((e = readdir(dir)))
    {
        if (!strstr(e->d_name, ".md"))
            continue;

        char name[MAX_PATH];
        strncpy(name, e->d_name, sizeof(name));
        char *dot = strstr(name, ".md");
        if (dot)
            *dot = 0;

        APPEND("<li><a href=\"/post/%s\">%s</a></li>", name, name);
    }

    closedir(dir);
    APPEND("</ul>%s", HTML_FOOTER);

    send_response(fd, "200 OK", "text/html", response);
    return;

overflow:
    send_response(fd, "500 Internal Server Error",
                  "text/html", "<h1>Response too large</h1>");
}

void serve_post(int fd, const char *dir, const char *name)
{
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s.md", dir, name);

    char *md = read_file(path);
    if (!md)
    {
        send_response(fd, "404 Not Found", "text/html",
                      "<h1>Post not found</h1>");
        return;
    }

    char *html = markdown_to_html(md);
    char title[256];
    get_title_from_markdown(md, title, sizeof(title));
    free(md);

    if (!html)
    {
        send_response(fd, "500 Internal Server Error",
                      "text/html", "<h1>Render failed</h1>");
        return;
    }

    char response[BUFFER_SIZE * 10];
    char *p = response;
    size_t remaining = sizeof(response);

    APPEND(HTML_HEADER, title);
    APPEND("%s", html);
    APPEND("%s", HTML_FOOTER);

    send_response(fd, "200 OK", "text/html", response);
    free(html);
    return;

overflow:
    free(html);
    send_response(fd, "500 Internal Server Error",
                  "text/html", "<h1>Post too large</h1>");
}

/* ================= SERVER ================= */

void *handle_client(void *arg)
{
    client_info_t *ci = arg;
    char buf[BUFFER_SIZE];
    read(ci->client_fd, buf, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    char method[8], path[MAX_PATH];
    sscanf(buf, "%7s %255s", method, path);

    if (!strcmp(method, "GET"))
    {
        if (!strcmp(path, "/"))
            serve_index(ci->client_fd, ci->blog_dir);
        else if (!strncmp(path, "/post/", 6))
            serve_post(ci->client_fd, ci->blog_dir, path + 6);
        else
            send_response(ci->client_fd, "404 Not Found",
                          "text/html", "<h1>Not found</h1>");
    }

    close(ci->client_fd);
    free(ci);
    return NULL;
}

/* ================= MAIN ================= */

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <port> <blog_dir>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    char *blog_dir = argv[2];
    mkdir(blog_dir, 0755);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)};

    bind(s, (struct sockaddr *)&addr, sizeof(addr));
    listen(s, 10);

    printf("Blog server running at http://localhost:%d/\n", port);

    while (1)
    {
        int fd = accept(s, NULL, NULL);
        client_info_t *ci = malloc(sizeof(*ci));
        ci->client_fd = fd;
        strncpy(ci->blog_dir, blog_dir, sizeof(ci->blog_dir));

        pthread_t t;
        pthread_create(&t, NULL, handle_client, ci);
        pthread_detach(t);
    }
}
