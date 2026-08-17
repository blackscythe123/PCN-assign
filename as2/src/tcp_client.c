/*
 * tcp_client.c
 * TCP client for Assignment 2, Part 1(a).
 * Prompts for a roll number, sends it to the server, prints whatever
 * comes back, and closes the connection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 6060
#define SERVER_IP "127.0.0.1"
#define BUF_SIZE 512

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(1);
    }

    char roll[32];
    printf("Enter Roll Number: ");
    fflush(stdout);
    if (fgets(roll, sizeof(roll), stdin) == NULL) {
        exit(1);
    }
    roll[strcspn(roll, "\r\n")] = '\0';

    send(sock, roll, strlen(roll), 0);

    char buf[BUF_SIZE];
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        printf("\n--- Server Response ---\n%s", buf);
    } else {
        printf("No response from server.\n");
    }

    close(sock); /* graceful close of the connection */
    return 0;
}
