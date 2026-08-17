/*
 * crc_receiver.c
 * Accepts a TCP connection from crc_sender.c, receives a binary frame
 * (data bits + CRC bits), recalculates the CRC using the same generator
 * G(x) = x^4 + x + 1 ("10011") and reports whether the frame is
 * error-free (remainder is all zero) or contains errors.
 *
 * Runs in a loop so it can be started once and handle multiple sender
 * runs (e.g. one clean frame, then one corrupted frame) in sequence.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define GENERATOR "10011"     /* G(x) = x^4 + x + 1, must match the sender */
#define PORT 6063

/* Mod-2 (XOR) polynomial division - same algorithm as the sender uses to
 * compute the CRC. Run on the FULL received frame (data + CRC), the
 * remainder should come out all zeros if nothing was corrupted. */
void mod2_divide(const char *dividend, int dividend_len,
                  const char *generator, int gen_len, char *remainder_out) {
    char temp[256];
    strcpy(temp, dividend);

    for (int i = 0; i <= dividend_len - gen_len; i++) {
        if (temp[i] == '1') {
            for (int j = 0; j < gen_len; j++) {
                temp[i + j] = (temp[i + j] == generator[j]) ? '0' : '1';
            }
        }
    }

    int rem_len = gen_len - 1;
    strncpy(remainder_out, temp + (dividend_len - rem_len), rem_len);
    remainder_out[rem_len] = '\0';
}

int is_all_zero(const char *bits) {
    for (int i = 0; bits[i] != '\0'; i++) {
        if (bits[i] != '0') return 0;
    }
    return 1;
}

int main(void) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    listen(server_fd, 5);
    printf("CRC receiver listening on TCP port %d ...\n", PORT);
    fflush(stdout);

    int gen_len = strlen(GENERATOR);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) { perror("accept"); continue; }

        printf("\nClient connected: %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        /* read until we see the newline that marks the end of the frame */
        char buf[512];
        int total = 0;
        int n;
        while ((n = recv(client_fd, buf + total, sizeof(buf) - 1 - total, 0)) > 0) {
            total += n;
            buf[total] = '\0';
            if (strchr(buf, '\n') != NULL) break;
        }
        buf[strcspn(buf, "\r\n")] = '\0'; /* strip the trailing newline */

        int frame_len = strlen(buf);
        printf("Received frame     : %s  (%d bits)\n", buf, frame_len);
        printf("Generator G(x)     : %s  (x^4 + x + 1)\n", GENERATOR);

        char remainder[16];
        mod2_divide(buf, frame_len, GENERATOR, gen_len, remainder);
        printf("Remainder          : %s\n", remainder);

        if (is_all_zero(remainder)) {
            printf("Verification result: FRAME IS ERROR-FREE (remainder = 0)\n");
        } else {
            printf("Verification result: FRAME CONTAINS ERRORS (remainder != 0)\n");
        }
        fflush(stdout);

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
