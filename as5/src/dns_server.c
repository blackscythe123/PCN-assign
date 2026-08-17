/* dns_server.c
 * Simulated DNS server over UDP. Holds a small in-memory hostname -> IP
 * table and answers lookup queries from dns_client.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 6062
#define BUF_SIZE 256
#define MAX_ENTRIES 10

typedef struct {
    char hostname[64];
    char ip[32];
} DnsEntry;

/* the "DNS table" -- just a fixed array, no hash map needed for this lab */
DnsEntry dns_table[MAX_ENTRIES] = {
    {"www.example.com",   "93.184.216.34"},
    {"mail.college.edu",  "10.20.30.40"},
    {"library.college.edu", "10.20.30.41"},
    {"portal.ssn.edu.in", "172.16.5.10"},
    {"chat.snowmail.com", "203.0.113.15"},
    {"www.gitplex.io",    "104.21.9.88"},
    {"cdn.mediahive.net", "198.51.100.7"},
    {"ftp.filedrop.org",  "192.0.2.55"},
    {"api.weatherbird.io","192.0.2.101"},
    {"blog.techmusings.dev","198.51.100.200"}
};
int entry_count = 10;

/* look up a hostname in the table, return the IP or NULL if not found */
char *lookup_hostname(char *hostname) {
    int i;
    for (i = 0; i < entry_count; i++) {
        if (strcmp(dns_table[i].hostname, hostname) == 0) {
            return dns_table[i].ip;
        }
    }
    return NULL;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buf[BUF_SIZE];
    char response[BUF_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("DNS server listening on UDP port %d ...\n", PORT);
    printf("Loaded %d hostname entries into DNS table.\n", entry_count);
    fflush(stdout);

    while (1) {
        int n = recvfrom(sockfd, buf, BUF_SIZE - 1, 0,
                          (struct sockaddr *)&client_addr, &client_len);
        if (n <= 0) continue;
        buf[n] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';

        printf("Query received from %s:%d -> \"%s\"\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buf);
        fflush(stdout);

        char *ip = lookup_hostname(buf);
        if (ip != NULL) {
            snprintf(response, sizeof(response), "FOUND %s", ip);
            printf("  -> resolved to %s\n", ip);
        } else {
            snprintf(response, sizeof(response), "NXDOMAIN");
            printf("  -> NXDOMAIN (not in table)\n");
        }
        fflush(stdout);

        sendto(sockfd, response, strlen(response), 0,
               (struct sockaddr *)&client_addr, client_len);
    }

    close(sockfd);
    return 0;
}
