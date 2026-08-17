/*
 * udp_client.c
 * UDP client for Assignment 2, Part 1(b).
 * Sends a roll number in a single datagram and prints whatever comes
 * back, then terminates - no connection to close.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 6061
#define SERVER_IP "127.0.0.1"
#define BUF_SIZE 512

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    char roll[32];
    printf("Enter Roll Number: ");
    fflush(stdout);
    if (fgets(roll, sizeof(roll), stdin) == NULL) {
        exit(1);
    }
    roll[strcspn(roll, "\r\n")] = '\0';

    sendto(sock, roll, strlen(roll), 0,
           (struct sockaddr *)&server_addr, sizeof(server_addr));

    char buf[BUF_SIZE];
    socklen_t addr_len = sizeof(server_addr);
    int n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                      (struct sockaddr *)&server_addr, &addr_len);
    if (n > 0) {
        buf[n] = '\0';
        printf("\n--- Server Response ---\n%s", buf);
    } else {
        printf("No response from server.\n");
    }

    close(sock);
    return 0;
}
