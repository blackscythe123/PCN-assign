#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT 5050
#define BACKLOG 5

static void strip_newline(char *s) {
    size_t n = strlen(s);
    if (n && s[n - 1] == '\n') s[n - 1] = '\0';
}

int main() {
    char myhost[256];
    gethostname(myhost, sizeof(myhost));
    printf("[server] running on host: %s\n", myhost);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    printf("[server] htonl(INADDR_ANY)=%u  htons(%d)=%u (network byte order)\n",
           servaddr.sin_addr.s_addr, PORT, servaddr.sin_port);
    printf("[server] ntohl() back = %u  ntohs() back = %u (host byte order)\n",
           ntohl(servaddr.sin_addr.s_addr), ntohs(servaddr.sin_port));

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(listenfd, BACKLOG) < 0) { perror("listen"); exit(1); }
    printf("[server] listening on port %d ... (waiting for the client to connect)\n", PORT);

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(listenfd, &readfds);
    struct timeval tv = {30, 0};
    int sel = select(listenfd + 1, &readfds, NULL, NULL, &tv);
    if (sel <= 0) {
        printf("[server] select() timed out waiting for a client, exiting.\n");
        close(listenfd);
        exit(1);
    }

    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    int connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
    if (connfd < 0) { perror("accept"); exit(1); }

    int flags = fcntl(connfd, F_GETFL, 0);
    printf("[server] fcntl(F_GETFL) on accepted socket = 0x%x (O_NONBLOCK bit=%d)\n",
           flags, (flags & O_NONBLOCK) ? 1 : 0);

    struct sockaddr_in peer;
    socklen_t peerlen = sizeof(peer);
    getpeername(connfd, (struct sockaddr *)&peer, &peerlen);
    printf("[server] getpeername() -> client connected from %s:%d\n",
           inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));

    struct hostent *client = gethostbyaddr(&peer.sin_addr, sizeof(peer.sin_addr), AF_INET);
    printf("[server] gethostbyaddr() -> %s\n\n",
           client ? client->h_name : "(no PTR record)");

    char inbuf[512], outbuf[512];

    /* turn 1: recv() / send() */
    bzero(inbuf, sizeof(inbuf));
    recv(connfd, inbuf, sizeof(inbuf) - 1, 0);
    strip_newline(inbuf);
    printf("them: %s\n", inbuf);
    printf("you: ");
    fflush(stdout);
    if (fgets(outbuf, sizeof(outbuf), stdin) == NULL) strcpy(outbuf, "bye\n");
    strip_newline(outbuf);
    printf("%s\n", outbuf);
    send(connfd, outbuf, strlen(outbuf), 0);

    /* turn 2: recvfrom() / sendto() */
    bzero(inbuf, sizeof(inbuf));
    recvfrom(connfd, inbuf, sizeof(inbuf) - 1, 0, NULL, NULL);
    strip_newline(inbuf);
    printf("them: %s\n", inbuf);
    printf("you: ");
    fflush(stdout);
    if (fgets(outbuf, sizeof(outbuf), stdin) == NULL) strcpy(outbuf, "bye\n");
    strip_newline(outbuf);
    printf("%s\n", outbuf);
    sendto(connfd, outbuf, strlen(outbuf), 0, NULL, 0);

    /* turn 3: read() / write() */
    bzero(inbuf, sizeof(inbuf));
    read(connfd, inbuf, sizeof(inbuf) - 1);
    strip_newline(inbuf);
    printf("them: %s\n", inbuf);
    printf("you: ");
    fflush(stdout);
    if (fgets(outbuf, sizeof(outbuf), stdin) == NULL) strcpy(outbuf, "bye\n");
    strip_newline(outbuf);
    printf("%s\n", outbuf);
    write(connfd, outbuf, strlen(outbuf));

    close(connfd);
    close(listenfd);
    printf("\n[server] close() called on both sockets. Server exiting.\n");
    return 0;
}
