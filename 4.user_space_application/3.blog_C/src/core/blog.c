/**
 * @file markdown_blog_server.c
 * @brief Simple HTTP server in C for serving Markdown blog posts
 * 
 * Features:
 * - Pure C implementation
 * - Serves static files and markdown
 * - Converts markdown to HTML on-the-fly
 * - Lightweight for Raspberry Pi 4
 * 
 * Build:
 *   gcc -o blog_server markdown_blog_server.c -pthread
 * 
 * Run:
 *   ./blog_server 8080 ./blog_posts
 * 
 * Access:
 *   http://localhost:8080/
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

#define BUFFER_SIZE 8192
#define MAX_PATH 256

typedef struct {
    int client_fd;
    char blog_dir[MAX_PATH];
} client_info_t;

// HTML template
const char *HTML_HEADER = 
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "    <meta charset=\"UTF-8\">\n"
    "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
    "    <title>%s</title>\n"
    "    <style>\n"
    "        body {\n"
    "            max-width: 800px;\n"
    "            margin: 40px auto;\n"
    "            padding: 0 20px;\n"
    "            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n"
    "            line-height: 1.6;\n"
    "            color: #333;\n"
    "        }\n"
    "        h1 { color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 10px; }\n"
    "        h2 { color: #34495e; margin-top: 30px; }\n"
    "        h3 { color: #5d6d7e; }\n"
    "        code {\n"
    "            background: #f4f4f4;\n"
    "            padding: 2px 6px;\n"
    "            border-radius: 3px;\n"
    "            font-family: 'Courier New', monospace;\n"
    "        }\n"
    "        pre {\n"
    "            background: #2d2d2d;\n"
    "            color: #f8f8f2;\n"
    "            padding: 15px;\n"
    "            border-radius: 5px;\n"
    "            overflow-x: auto;\n"
    "        }\n"
    "        pre code { background: none; padding: 0; color: #f8f8f2; }\n"
    "        a { color: #3498db; text-decoration: none; }\n"
    "        a:hover { text-decoration: underline; }\n"
    "        .post-list { list-style: none; padding: 0; }\n"
    "        .post-list li {\n"
    "            margin: 15px 0;\n"
    "            padding: 15px;\n"
    "            border-left: 3px solid #3498db;\n"
    "            background: #f8f9fa;\n"
    "            border-radius: 3px;\n"
    "        }\n"
    "        .post-meta { color: #7f8c8d; font-size: 0.9em; }\n"
    "        nav { margin-bottom: 20px; }\n"
    "        blockquote {\n"
    "            border-left: 4px solid #3498db;\n"
    "            margin: 20px 0;\n"
    "            padding: 10px 20px;\n"
    "            background: #f8f9fa;\n"
    "        }\n"
    "    </style>\n"
    "</head>\n"
    "<body>\n"
    "    <nav><a href=\"/\">← Home</a></nav>\n";

const char *HTML_FOOTER = 
    "</body>\n"
    "</html>\n";

// Simple markdown to HTML converter
void markdown_to_html(const char *markdown, char *html, size_t html_size) {
    const char *p = markdown;
    char *out = html;
    size_t remaining = html_size - 1;
    int in_code_block = 0;
    int in_paragraph = 0;
    
    while (*p && remaining > 100) {
        // Code blocks (```)
        if (strncmp(p, "```", 3) == 0) {
            if (in_code_block) {
                out += snprintf(out, remaining, "</code></pre>\n");
                in_code_block = 0;
            } else {
                if (in_paragraph) {
                    out += snprintf(out, remaining, "</p>\n");
                    in_paragraph = 0;
                }
                out += snprintf(out, remaining, "<pre><code>");
                in_code_block = 1;
            }
            remaining = html_size - (out - html);
            p += 3;
            while (*p && *p != '\n') p++; // Skip language identifier
            if (*p) p++;
            continue;
        }
        
        if (in_code_block) {
            *out++ = *p++;
            remaining--;
            continue;
        }
        
        // Headers
        if (*p == '#' && (p == markdown || *(p-1) == '\n')) {
            if (in_paragraph) {
                out += snprintf(out, remaining, "</p>\n");
                in_paragraph = 0;
            }
            
            int level = 0;
            while (*p == '#' && level < 6) { level++; p++; }
            while (*p == ' ') p++;
            
            out += snprintf(out, remaining, "<h%d>", level);
            remaining = html_size - (out - html);
            
            while (*p && *p != '\n') {
                *out++ = *p++;
                remaining--;
            }
            out += snprintf(out, remaining, "</h%d>\n", level);
            remaining = html_size - (out - html);
            if (*p) p++;
            continue;
        }
        
        // Inline code
        if (*p == '`') {
            p++;
            out += snprintf(out, remaining, "<code>");
            remaining = html_size - (out - html);
            while (*p && *p != '`') {
                *out++ = *p++;
                remaining--;
            }
            out += snprintf(out, remaining, "</code>");
            remaining = html_size - (out - html);
            if (*p) p++;
            continue;
        }
        
        // Bold **text**
        if (*p == '*' && *(p+1) == '*') {
            p += 2;
            out += snprintf(out, remaining, "<strong>");
            remaining = html_size - (out - html);
            while (*p && !(*p == '*' && *(p+1) == '*')) {
                *out++ = *p++;
                remaining--;
            }
            out += snprintf(out, remaining, "</strong>");
            remaining = html_size - (out - html);
            if (*p) p += 2;
            continue;
        }
        
        // Line breaks
        if (*p == '\n') {
            if (*(p+1) == '\n') {
                if (in_paragraph) {
                    out += snprintf(out, remaining, "</p>\n");
                    in_paragraph = 0;
                    remaining = html_size - (out - html);
                }
                p++;
            } else if (!in_paragraph && *(p+1) != '#' && *(p+1) != '\n') {
                out += snprintf(out, remaining, "<p>");
                in_paragraph = 1;
                remaining = html_size - (out - html);
            }
        }
        
        *out++ = *p++;
        remaining--;
    }
    
    if (in_paragraph) {
        out += snprintf(out, remaining, "</p>\n");
    }
    
    *out = '\0';
}

// Read file content
char* read_file(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);
    
    return content;
}

// Get first line as title
void get_title_from_markdown(const char *content, char *title, size_t title_size) {
    const char *p = content;
    
    // Skip leading whitespace
    while (*p && isspace(*p)) p++;
    
    // Skip # characters
    while (*p == '#') p++;
    while (*p && isspace(*p)) p++;
    
    // Copy title
    size_t i = 0;
    while (*p && *p != '\n' && i < title_size - 1) {
        title[i++] = *p++;
    }
    title[i] = '\0';
    
    if (title[0] == '\0') {
        strncpy(title, "Untitled", title_size);
    }
}

// Send HTTP response
void send_response(int client_fd, const char *status, const char *content_type, const char *body) {
    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, content_type, strlen(body));
    
    write(client_fd, header, strlen(header));
    write(client_fd, body, strlen(body));
}

// List all markdown posts
void serve_index(int client_fd, const char *blog_dir) {
    DIR *dir = opendir(blog_dir);
    if (!dir) {
        send_response(client_fd, "500 Internal Server Error", "text/html", 
                     "<h1>Error: Cannot open blog directory</h1>");
        return;
    }
    
    char response[BUFFER_SIZE * 4];
    char *p = response;
    size_t remaining = sizeof(response);
    
    p += snprintf(p, remaining, HTML_HEADER, "My Blog");
    remaining = sizeof(response) - (p - response);
    
    p += snprintf(p, remaining, "<h1>My Blog</h1>\n<ul class=\"post-list\">\n");
    remaining = sizeof(response) - (p - response);
    
    struct dirent *entry;
    int post_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".md")) {
            char filepath[MAX_PATH];
            snprintf(filepath, sizeof(filepath), "%s/%s", blog_dir, entry->d_name);
            
            // Get file modification time
            struct stat st;
            stat(filepath, &st);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&st.st_mtime));
            
            // Get title from file
            char *content = read_file(filepath);
            char title[256];
            if (content) {
                get_title_from_markdown(content, title, sizeof(title));
                free(content);
            } else {
                strncpy(title, entry->d_name, sizeof(title));
            }
            
            // Remove .md extension for URL
            char post_name[MAX_PATH];
            strncpy(post_name, entry->d_name, sizeof(post_name));
            char *ext = strstr(post_name, ".md");
            if (ext) *ext = '\0';
            
            p += snprintf(p, remaining, 
                "<li>\n"
                "    <a href=\"/post/%s\"><strong>%s</strong></a><br>\n"
                "    <span class=\"post-meta\">%s</span>\n"
                "</li>\n",
                post_name, title, time_str);
            remaining = sizeof(response) - (p - response);
            
            post_count++;
        }
    }
    
    closedir(dir);
    
    if (post_count == 0) {
        p += snprintf(p, remaining, 
            "<p>No posts yet. Add .md files to the blog directory.</p>\n");
        remaining = sizeof(response) - (p - response);
    }
    
    p += snprintf(p, remaining, "</ul>\n");
    p += snprintf(p, remaining, HTML_FOOTER);
    
    send_response(client_fd, "200 OK", "text/html", response);
}

// Serve individual post
void serve_post(int client_fd, const char *blog_dir, const char *post_name) {
    char filepath[MAX_PATH];
    snprintf(filepath, sizeof(filepath), "%s/%s.md", blog_dir, post_name);
    
    char *markdown = read_file(filepath);
    if (!markdown) {
        send_response(client_fd, "404 Not Found", "text/html", 
                     "<h1>404 - Post Not Found</h1>");
        return;
    }
    
    // Convert markdown to HTML
    char *html_body = malloc(BUFFER_SIZE * 8);
    if (!html_body) {
        free(markdown);
        send_response(client_fd, "500 Internal Server Error", "text/html", 
                     "<h1>Server Error</h1>");
        return;
    }
    
    markdown_to_html(markdown, html_body, BUFFER_SIZE * 8);
    
    // Get title
    char title[256];
    get_title_from_markdown(markdown, title, sizeof(title));
    free(markdown);
    
    // Build complete HTML
    char *response = malloc(BUFFER_SIZE * 10);
    if (!response) {
        free(html_body);
        send_response(client_fd, "500 Internal Server Error", "text/html", 
                     "<h1>Server Error</h1>");
        return;
    }
    
    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header), HTML_HEADER, title);
    
    snprintf(response, BUFFER_SIZE * 10, "%s%s%s", header, html_body, HTML_FOOTER);
    
    send_response(client_fd, "200 OK", "text/html", response);
    
    free(html_body);
    free(response);
}

// Handle client request
void* handle_client(void *arg) {
    client_info_t *info = (client_info_t*)arg;
    int client_fd = info->client_fd;
    char blog_dir[MAX_PATH];
    strncpy(blog_dir, info->blog_dir, sizeof(blog_dir));
    free(info);
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        // Parse request
        char method[16], path[MAX_PATH], protocol[16];
        sscanf(buffer, "%s %s %s", method, path, protocol);
        
        printf("[%s] %s\n", method, path);
        
        if (strcmp(method, "GET") == 0) {
            if (strcmp(path, "/") == 0) {
                serve_index(client_fd, blog_dir);
            } else if (strncmp(path, "/post/", 6) == 0) {
                serve_post(client_fd, blog_dir, path + 6);
            } else {
                send_response(client_fd, "404 Not Found", "text/html", 
                             "<h1>404 - Not Found</h1>");
            }
        }
    }
    
    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <blog_directory>\n", argv[0]);
        fprintf(stderr, "Example: %s 8080 ./blog_posts\n", argv[0]);
        return 1;
    }
    
    int port = atoi(argv[1]);
    char *blog_dir = argv[2];
    
    // Create blog directory if it doesn't exist
    mkdir(blog_dir, 0755);
    
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    
    // Allow reuse of address
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    
    // Listen
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }
    
    printf("========================================\n");
    printf("Markdown Blog Server (C Edition)\n");
    printf("========================================\n");
    printf("Port: %d\n", port);
    printf("Blog directory: %s\n", blog_dir);
    printf("\nPut your .md files in: %s/\n", blog_dir);
    printf("\nAccess your blog at:\n");
    printf("  http://localhost:%d/\n", port);
    printf("\nPress Ctrl+C to stop\n");
    printf("========================================\n\n");
    
    // Accept connections
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        
        // Handle in new thread
        client_info_t *info = malloc(sizeof(client_info_t));
        info->client_fd = client_fd;
        strncpy(info->blog_dir, blog_dir, sizeof(info->blog_dir));
        
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, info);
        pthread_detach(thread);
    }
    
    close(server_fd);
    return 0;
}