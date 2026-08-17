/*
 * http_client.c
 *
 * Assignment 3 - Implementation of HTTP Client using TCP Sockets.
 *
 * A simple TCP socket based HTTP/1.1 GET client.
 *   - Asks the user for a URL (http://host[:port]/path)
 *   - Parses out the host, port and path
 *   - Connects to the server with a TCP socket
 *   - Sends a plain HTTP GET request
 *   - Reads the raw response, separates the headers from the body
 *   - Saves the body to a local file (downloaded_<filename>)
 *   - Prints connection time, download time, bytes transferred and
 *     throughput so the communication performance can be analysed.
 *
 * Works for any plain HTTP server (part a) and was specifically tested
 * against a Python "python3 -m http.server 8000" instance (part b).
 *
 * Only http:// URLs are supported (no TLS) - that is fine because the
 * mandatory part of this assignment only needs plain HTTP.
 */

#define _GNU_SOURCE /* for memmem() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_URL_LEN   2048
#define MAX_HOST_LEN  256
#define MAX_PATH_LEN  1024
#define RECV_CHUNK    8192

/* Returns elapsed time in milliseconds between two timespec samples. */
static double elapsed_ms(struct timespec start, struct timespec end)
{
    double sec_part  = (double)(end.tv_sec - start.tv_sec) * 1000.0;
    double nsec_part = (double)(end.tv_nsec - start.tv_nsec) / 1000000.0;
    return sec_part + nsec_part;
}

/*
 * Breaks a URL of the form  http://host[:port]/path  into its pieces.
 * host, path must be caller-supplied buffers. port is filled with the
 * numeric port (default 80 if none was given in the URL).
 * Returns 0 on success, -1 if the URL is not a supported http:// URL.
 */
static int parse_url(const char *url, char *host, size_t host_len,
                      int *port, char *path, size_t path_len)
{
    const char *p = url;

    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        fprintf(stderr, "This client only supports plain http:// URLs.\n");
        return -1;
    } else {
        fprintf(stderr, "URL must start with http://\n");
        return -1;
    }

    /* Everything up to the next '/' is host[:port] */
    const char *slash = strchr(p, '/');
    const char *hostport_end = slash ? slash : p + strlen(p);

    char hostport[MAX_HOST_LEN + 16];
    size_t hp_len = (size_t)(hostport_end - p);
    if (hp_len == 0 || hp_len >= sizeof(hostport)) {
        fprintf(stderr, "Invalid host in URL.\n");
        return -1;
    }
    memcpy(hostport, p, hp_len);
    hostport[hp_len] = '\0';

    /* Split hostport into host and optional :port */
    char *colon = strchr(hostport, ':');
    if (colon) {
        *colon = '\0';
        *port = atoi(colon + 1);
        if (*port <= 0) *port = 80;
    } else {
        *port = 80;
    }
    strncpy(host, hostport, host_len - 1);
    host[host_len - 1] = '\0';

    /* Path is whatever follows the host[:port], default to "/" */
    if (slash) {
        strncpy(path, slash, path_len - 1);
        path[path_len - 1] = '\0';
    } else {
        strncpy(path, "/", path_len - 1);
        path[path_len - 1] = '\0';
    }

    return 0;
}

/* Picks a local file name to save the downloaded body under. */
static void make_output_filename(const char *path, char *out, size_t out_len)
{
    const char *last_slash = strrchr(path, '/');
    const char *basename = last_slash ? last_slash + 1 : path;

    if (basename[0] == '\0') {
        snprintf(out, out_len, "downloaded_index.html");
    } else {
        snprintf(out, out_len, "downloaded_%s", basename);
    }
}

int main(void)
{
    char url[MAX_URL_LEN];
    char host[MAX_HOST_LEN];
    char path[MAX_PATH_LEN];
    int port;

    printf("Enter URL: ");
    if (!fgets(url, sizeof(url), stdin)) {
        fprintf(stderr, "Failed to read URL.\n");
        return 1;
    }
    url[strcspn(url, "\r\n")] = '\0'; /* strip trailing newline */

    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path)) != 0) {
        return 1;
    }

    printf("Host: %s\n", host);
    printf("Port: %d\n", port);
    printf("Path: %s\n", path);

    /* ---- resolve host and open a TCP connection ---- */
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int gai_err = getaddrinfo(host, port_str, &hints, &res);
    if (gai_err != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(gai_err));
        return 1;
    }

    int sockfd = -1;
    struct timespec t_conn_start, t_conn_end;
    clock_gettime(CLOCK_MONOTONIC, &t_conn_start);

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) break; /* success */
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);

    if (sockfd == -1) {
        fprintf(stderr, "Could not connect to %s:%d\n", host, port);
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t_conn_end);

    /* ---- build and send the GET request ---- */
    char request[MAX_PATH_LEN + MAX_HOST_LEN + 128];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host);

    printf("\n--- Request sent ---\n%s", request);

    struct timespec t_req_sent, t_first_byte, t_done;
    clock_gettime(CLOCK_MONOTONIC, &t_req_sent);

    if (send(sockfd, request, strlen(request), 0) < 0) {
        perror("send");
        close(sockfd);
        return 1;
    }

    /* ---- read the whole response into a growable buffer ---- */
    size_t capacity = RECV_CHUNK * 4;
    size_t total = 0;
    char *buf = malloc(capacity);
    if (!buf) {
        fprintf(stderr, "Out of memory.\n");
        close(sockfd);
        return 1;
    }

    int got_first_byte = 0;
    ssize_t n;
    while ((n = recv(sockfd, buf + total, capacity - total, 0)) > 0) {
        if (!got_first_byte) {
            clock_gettime(CLOCK_MONOTONIC, &t_first_byte);
            got_first_byte = 1;
        }
        total += (size_t)n;
        if (total == capacity) {
            capacity *= 2;
            char *nbuf = realloc(buf, capacity);
            if (!nbuf) {
                fprintf(stderr, "Out of memory while receiving.\n");
                free(buf);
                close(sockfd);
                return 1;
            }
            buf = nbuf;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t_done);
    close(sockfd);

    if (n < 0) {
        perror("recv");
        free(buf);
        return 1;
    }
    if (!got_first_byte) {
        fprintf(stderr, "Server closed the connection with no data.\n");
        free(buf);
        return 1;
    }

    /* ---- split headers from body on the blank line ---- */
    char *header_end = memmem(buf, total, "\r\n\r\n", 4);
    if (!header_end) {
        fprintf(stderr, "Malformed response: no header/body separator found.\n");
        free(buf);
        return 1;
    }
    size_t header_len = (size_t)(header_end - buf);
    char *body = header_end + 4;
    size_t body_len = total - header_len - 4;

    /* Print the status line and headers so we can see what came back. */
    printf("\n--- Response headers ---\n%.*s\n", (int)header_len, buf);

    /* Grab the status line (first line of the headers) for a quick check. */
    char status_line[256] = "";
    char *first_eol = memchr(buf, '\n', header_len);
    if (first_eol) {
        size_t slen = (size_t)(first_eol - buf);
        if (slen >= sizeof(status_line)) slen = sizeof(status_line) - 1;
        memcpy(status_line, buf, slen);
        status_line[slen] = '\0';
    }

    /* ---- save the body to a local file ---- */
    char out_name[300];
    make_output_filename(path, out_name, sizeof(out_name));

    FILE *fp = fopen(out_name, "wb");
    if (!fp) {
        perror("fopen");
        free(buf);
        return 1;
    }
    fwrite(body, 1, body_len, fp);
    fclose(fp);

    /* ---- performance analysis ---- */
    double connect_ms  = elapsed_ms(t_conn_start, t_conn_end);
    double response_ms = elapsed_ms(t_req_sent, t_first_byte); /* time to first byte */
    double download_ms = elapsed_ms(t_req_sent, t_done);       /* full transfer time */
    double throughput_Bps = (download_ms > 0.0)
                                 ? (double)body_len / (download_ms / 1000.0)
                                 : 0.0;

    printf("\n--- Performance ---\n");
    printf("Status line       : %s\n", status_line);
    printf("Connection time   : %.3f ms\n", connect_ms);
    printf("Response time     : %.3f ms (time to first byte)\n", response_ms);
    printf("Download time     : %.3f ms (full response)\n", download_ms);
    printf("Body size         : %zu bytes\n", body_len);
    printf("Throughput        : %.2f bytes/sec (%.2f KB/s)\n",
           throughput_Bps, throughput_Bps / 1024.0);
    printf("Saved body to     : %s\n", out_name);

    free(buf);
    return 0;
}
