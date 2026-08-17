/* dns_client.c
 * Simulated DNS client over UDP. Reads hostnames one per line from stdin,
 * checks a small local cache first, and only falls back to a UDP query to
 * dns_server.c on a cache miss. Also does basic input validation and a
 * recvfrom() timeout so it never hangs forever if the server is down.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_IP "127.0.0.1"
#define PORT 6062
#define BUF_SIZE 256
#define MAX_CACHE 20
#define TIMEOUT_SEC 2

typedef struct {
    char hostname[64];
    char ip[32];
} CacheEntry;

CacheEntry cache[MAX_CACHE];
int cache_count = 0;

/* checked BEFORE any UDP send -- this is the caching requirement */
char *cache_lookup(char *hostname) {
    int i;
    for (i = 0; i < cache_count; i++) {
        if (strcmp(cache[i].hostname, hostname) == 0) {
            return cache[i].ip;
        }
    }
    return NULL;
}

void cache_add(char *hostname, char *ip) {
    if (cache_count >= MAX_CACHE) {
        /* cache full: just overwrite the oldest slot, good enough for a lab */
        cache_count = 0;
    }
    strncpy(cache[cache_count].hostname, hostname, sizeof(cache[cache_count].hostname) - 1);
    cache[cache_count].hostname[sizeof(cache[cache_count].hostname) - 1] = '\0';
    strncpy(cache[cache_count].ip, ip, sizeof(cache[cache_count].ip) - 1);
    cache[cache_count].ip[sizeof(cache[cache_count].ip) - 1] = '\0';
    cache_count++;
}

/* very simple hostname format check -- not a full RFC 1035 validator, just
 * enough to reject the obviously-bad input the assignment asks for */
int is_valid_hostname(char *hostname) {
    int len = strlen(hostname);
    int i;
    int has_dot = 0;

    if (len == 0 || len > 63) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        char c = hostname[i];
        if (isalnum((unsigned char)c) || c == '.' || c == '-') {
            if (c == '.') has_dot = 1;
        } else {
            return 0; /* invalid character */
        }
    }

    if (!has_dot) {
        return 0; /* no dot -- not a plausible hostname */
    }

    return 1;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char line[BUF_SIZE];
    struct timeval tv;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    /* recvfrom() timeout so the client never blocks forever on a dead server */
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    printf("DNS client ready. Enter hostnames to resolve (one per line, blank line/EOF to quit).\n");
    fflush(stdout);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';

        if (strlen(line) == 0) {
            break;
        }

        printf("\n> Query: \"%s\"\n", line);

        /* 1. validate format BEFORE touching the network */
        if (!is_valid_hostname(line)) {
            printf("  ERROR: invalid hostname format, not sending to server.\n");
            fflush(stdout);
            continue;
        }

        /* 2. check the local cache first */
        char *cached_ip = cache_lookup(line);
        if (cached_ip != NULL) {
            printf("  CACHE HIT: %s -> %s (no UDP query sent)\n", line, cached_ip);
            fflush(stdout);
            continue;
        }

        /* 3. cache miss -- go ask the server */
        printf("  CACHE MISS: querying DNS server...\n");
        fflush(stdout);

        if (sendto(sockfd, line, strlen(line), 0,
                   (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("sendto");
            continue;
        }

        char response[BUF_SIZE];
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);

        int n = recvfrom(sockfd, response, BUF_SIZE - 1, 0,
                          (struct sockaddr *)&from_addr, &from_len);

        if (n < 0) {
            /* SO_RCVTIMEO expired with no reply -- this is the timeout path */
            printf("  TIMEOUT: no response from DNS server within %d seconds.\n", TIMEOUT_SEC);
            fflush(stdout);
            continue;
        }

        response[n] = '\0';

        if (strncmp(response, "FOUND ", 6) == 0) {
            char *ip = response + 6;
            printf("  RESOLVED: %s -> %s\n", line, ip);
            cache_add(line, ip);
        } else if (strcmp(response, "NXDOMAIN") == 0) {
            printf("  NXDOMAIN: \"%s\" does not exist in the DNS table.\n", line);
        } else {
            printf("  Unrecognised server response: %s\n", response);
        }
        fflush(stdout);
    }

    printf("\nClient exiting.\n");
    close(sockfd);
    return 0;
}
