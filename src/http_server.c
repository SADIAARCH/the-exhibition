#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define CLOSE_SOCKET(s) closesocket(s)
#define INVALID_SOCK INVALID_SOCKET
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int socket_t;
#define CLOSE_SOCKET(s) close(s)
#define INVALID_SOCK (-1)
#endif

#include "http_server.h"
#include "db.h"
#include "json.h"
#include "artwork.h"

#define BUF_SIZE 65536

static void send_all(socket_t client, const char *data, long len) {
    long sent_total = 0;
    while (sent_total < len) {
        int n = (int)send(client, data + sent_total, (int)(len - sent_total), 0);
        if (n <= 0) break;
        sent_total += n;
    }
}

static void send_response(socket_t client, int status, const char *status_text,
                           const char *content_type, const char *body, long body_len) {
    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, content_type, body_len);
    send_all(client, header, hlen);
    if (body_len > 0) send_all(client, body, body_len);
}

static void send_json(socket_t client, int status, const char *status_text, char *json_body) {
    send_response(client, status, status_text, "application/json", json_body, (long)strlen(json_body));
    free(json_body);
}

static void send_file(socket_t client, const char *path, const char *content_type) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        const char *msg = "{\"error\":\"not found\"}";
        send_response(client, 404, "Not Found", "application/json", msg, (long)strlen(msg));
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc(size > 0 ? (size_t)size : 1);
    if (size > 0) fread(data, 1, (size_t)size, f);
    fclose(f);
    send_response(client, 200, "OK", content_type, data, size);
    free(data);
}

/* Case-insensitive substring search for HTTP header lookup. */
static const char *find_ci(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++) {
        size_t i = 0;
        while (i < nlen && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == nlen) return p;
    }
    return NULL;
}

/* Extracts a numeric id from a path like "/api/artworks/7". Returns -1 if none. */
static int path_id(const char *path, const char *prefix) {
    size_t plen = strlen(prefix);
    if (strncmp(path, prefix, plen) != 0) return -1;
    const char *rest = path + plen;
    if (*rest != '/') return -1;
    rest++;
    if (*rest == '\0') return -1;
    return atoi(rest);
}

static void handle_request(socket_t client, const char *public_dir) {
    char buf[BUF_SIZE];
    int received = 0;
    const char *body_start = NULL;

    /* Read until end of HTTP headers ("\r\n\r\n") or buffer full. */
    while (received < BUF_SIZE - 1) {
        int n = (int)recv(client, buf + received, BUF_SIZE - 1 - received, 0);
        if (n <= 0) break;
        received += n;
        buf[received] = '\0';
        body_start = strstr(buf, "\r\n\r\n");
        if (body_start) break;
    }
    if (received <= 0) return;

    char method[8], path[256];
    if (sscanf(buf, "%7s %255s", method, path) != 2) return;

    /* Strip any query string from path. */
    char *q = strchr(path, '?');
    if (q) *q = '\0';

    /* Find Content-Length + body if present (for POST/PUT), reading more if needed. */
    long content_length = 0;
    const char *cl = find_ci(buf, "Content-Length:");
    if (cl) content_length = atol(cl + strlen("Content-Length:"));
    if (content_length < 0) content_length = 0;

    char *body = NULL;
    if (body_start) {
        body_start += 4;
        long have = (long)(received - (body_start - buf));
        body = malloc((size_t)content_length + 1);
        long to_copy = have > content_length ? content_length : have;
        if (to_copy > 0) memcpy(body, body_start, (size_t)to_copy);
        have = to_copy > 0 ? to_copy : 0;
        while (have < content_length) {
            int more = (int)recv(client, body + have, (int)(content_length - have), 0);
            if (more <= 0) break;
            have += more;
        }
        body[content_length] = '\0';
    }

    if (strcmp(method, "OPTIONS") == 0) {
        send_response(client, 204, "No Content", "text/plain", "", 0);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/artworks") == 0) {
        int count;
        Artwork *list = db_get_all(&count);
        char *json = json_from_artworks(list, count);
        free(list);
        send_json(client, 200, "OK", json);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/artworks/stats") == 0) {
        int count;
        Artwork *list = db_get_all(&count);
        char *json = json_stats_from_artworks(list, count);
        free(list);
        send_json(client, 200, "OK", json);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/artworks") == 0 && body) {
        Artwork a;
        json_parse_artwork(body, &a);
        if (a.title[0] == '\0' || a.artist[0] == '\0') {
            char *err = strdup("{\"error\":\"title and artist are required\"}");
            send_json(client, 400, "Bad Request", err);
        } else {
            int new_id = db_add_artwork(&a);
            if (new_id < 0) {
                char *err = strdup("{\"error\":\"insert failed\"}");
                send_json(client, 400, "Bad Request", err);
            } else {
                char *json = malloc(32);
                snprintf(json, 32, "{\"id\":%d}", new_id);
                send_json(client, 201, "Created", json);
            }
        }
    } else if (strcmp(method, "PUT") == 0 && strncmp(path, "/api/artworks/", 14) == 0 && body) {
        int id = path_id(path, "/api/artworks");
        Artwork a;
        json_parse_artwork(body, &a);
        if (a.title[0] == '\0' || a.artist[0] == '\0') {
            char *err = strdup("{\"error\":\"title and artist are required\"}");
            send_json(client, 400, "Bad Request", err);
        } else {
            int rc = db_update_artwork(id, &a);
            char *json = strdup(rc == 0 ? "{\"status\":\"updated\"}" : "{\"error\":\"update failed\"}");
            send_json(client, rc == 0 ? 200 : 404, rc == 0 ? "OK" : "Not Found", json);
        }
    } else if (strcmp(method, "DELETE") == 0 && strncmp(path, "/api/artworks/", 14) == 0) {
        int id = path_id(path, "/api/artworks");
        int rc = db_delete_artwork(id);
        char *json = strdup(rc == 0 ? "{\"status\":\"deleted\"}" : "{\"error\":\"delete failed\"}");
        send_json(client, rc == 0 ? 200 : 404, rc == 0 ? "OK" : "Not Found", json);
    } else if (strcmp(method, "GET") == 0) {
        /* Static file serving for the frontend. */
        if (strstr(path, "..")) {
            const char *msg = "{\"error\":\"forbidden\"}";
            send_response(client, 403, "Forbidden", "application/json", msg, (long)strlen(msg));
        } else {
            char filepath[512];
            if (strcmp(path, "/") == 0) {
                snprintf(filepath, sizeof(filepath), "%s/index.html", public_dir);
            } else {
                snprintf(filepath, sizeof(filepath), "%s%s", public_dir, path);
            }
            const char *ct = "text/html";
            if (strstr(filepath, ".css")) ct = "text/css";
            else if (strstr(filepath, ".js")) ct = "application/javascript";
            send_file(client, filepath, ct);
        }
    } else {
        const char *msg = "{\"error\":\"not found\"}";
        send_response(client, 404, "Not Found", "application/json", msg, (long)strlen(msg));
    }

    free(body);
}

void http_server_run(int port, const char *public_dir) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        exit(1);
    }
#endif

    socket_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCK) {
        perror("socket failed");
        exit(1);
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        CLOSE_SOCKET(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
    listen(server_fd, 16);
    printf("Art Exhibition server running on http://localhost:%d\n", port);
    fflush(stdout);

    while (1) {
        socket_t client = accept(server_fd, NULL, NULL);
        if (client == INVALID_SOCK) continue;
        handle_request(client, public_dir);
        CLOSE_SOCKET(client);
    }
}
