#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT 5050

static void strip_newline(char *s) {
    size_t n = strlen(s);
    if (n && s[n - 1] == '\n') s[n - 1] = '\0';
}

int main(int argc, char *argv[]) {
    const char *server = (argc > 1) ? argv[1] : "127.0.0.1";

    char myhost[256];
    gethostname(myhost, sizeof(myhost));
    printf("[client] running on host: %s, connecting to %s\n", myhost, server);

    struct hostent *he = gethostbyname(server);
    if (he == NULL) { fprintf(stderr, "gethostbyname failed\n"); exit(1); }
    printf("[client] gethostbyname(\"%s\") -> %s\n", server,
           inet_ntoa(*(struct in_addr *)he->h_addr_list[0]));

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    memcpy(&servaddr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect"); exit(1);
    }
    printf("[client] connect() succeeded on port %d\n\n", PORT);

    char inbuf[512], outbuf[512];

    /* turn 1: send() / recv() */
    printf("you: ");
    fflush(stdout);
    if (fgets(outbuf, sizeof(outbuf), stdin) == NULL) strcpy(outbuf, "bye\n");
    strip_newline(outbuf);
    printf("%s\n", outbuf);
    send(sockfd, outbuf, strlen(outbuf), 0);
    bzero(inbuf, sizeof(inbuf));
    recv(sockfd, inbuf, sizeof(inbuf) - 1, 0);
    strip_newline(inbuf);
    printf("them: %s\n", inbuf);

    /* turn 2: sendto() / recvfrom() */
    printf("you: ");
    fflush(stdout);
    if (fgets(outbuf, sizeof(outbuf), stdin) == NULL) strcpy(outbuf, "bye\n");
    strip_newline(outbuf);
    printf("%s\n", outbuf);
    sendto(sockfd, outbuf, strlen(outbuf), 0, NULL, 0);
    bzero(inbuf, sizeof(inbuf));
    recvfrom(sockfd, inbuf, sizeof(inbuf) - 1, 0, NULL, NULL);
    strip_newline(inbuf);
    printf("them: %s\n", inbuf);

    /* turn 3: write() / read() */
    printf("you: ");
    fflush(stdout);
    if (fgets(outbuf, sizeof(outbuf), stdin) == NULL) strcpy(outbuf, "bye\n");
    strip_newline(outbuf);
    printf("%s\n", outbuf);
    write(sockfd, outbuf, strlen(outbuf));
    bzero(inbuf, sizeof(inbuf));
    read(sockfd, inbuf, sizeof(inbuf) - 1);
    strip_newline(inbuf);
    printf("them: %s\n", inbuf);

    close(sockfd);
    printf("\n[client] close() called. Client exiting.\n");
    return 0;
}
