/*
 * crc_sender.c
 * Converts a message to binary, computes a CRC using generator
 * G(x) = x^4 + x + 1 (bit pattern "10011"), appends the CRC to the data,
 * and sends the resulting frame to crc_receiver.c over a TCP socket.
 *
 * Usage: ./crc_sender [message] [corrupt]
 *   message  - text to send (default "HI")
 *   corrupt  - if the literal word "corrupt" is passed as the 2nd argument,
 *              a bit in the frame is flipped right before sending, so the
 *              receiver should detect an error.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define GENERATOR "10011"     /* G(x) = x^4 + x + 1 */
#define SERVER_IP "127.0.0.1"
#define PORT 6063

/* Converts each character of "data" into its 8-bit binary equivalent
 * and writes the concatenated bit string into "binary_out". */
void data_to_binary(const char *data, char *binary_out) {
    int idx = 0;
    for (int i = 0; data[i] != '\0'; i++) {
        unsigned char c = (unsigned char)data[i];
        for (int b = 7; b >= 0; b--) {
            binary_out[idx++] = ((c >> b) & 1) ? '1' : '0';
        }
    }
    binary_out[idx] = '\0';
}

/* Mod-2 (XOR) polynomial division. Divides "dividend" (length dividend_len)
 * by "generator" (length gen_len) and writes the (gen_len-1)-bit remainder
 * into "remainder_out". This is the core CRC computation loop. */
void mod2_divide(const char *dividend, int dividend_len,
                  const char *generator, int gen_len, char *remainder_out) {
    char temp[256];
    strcpy(temp, dividend);

    for (int i = 0; i <= dividend_len - gen_len; i++) {
        /* only XOR when the leading bit at this position is 1 -
         * that's how binary long division skips over leading zeros */
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

int main(int argc, char *argv[]) {
    const char *message = (argc > 1) ? argv[1] : "HI";
    int corrupt = (argc > 2 && strcmp(argv[2], "corrupt") == 0);

    char binary_data[256];
    data_to_binary(message, binary_data);
    int data_len = strlen(binary_data);

    int gen_len = strlen(GENERATOR);
    int deg = gen_len - 1; /* number of CRC bits = degree of generator */

    /* dividend = binary_data followed by "deg" zero bits */
    char dividend[256];
    strcpy(dividend, binary_data);
    for (int i = 0; i < deg; i++) strcat(dividend, "0");

    char crc[16];
    mod2_divide(dividend, data_len + deg, GENERATOR, gen_len, crc);

    /* frame to transmit = original data bits + computed CRC bits */
    char frame[300];
    snprintf(frame, sizeof(frame), "%s%s", binary_data, crc);

    printf("Message            : \"%s\"\n", message);
    printf("Binary data        : %s\n", binary_data);
    printf("Generator G(x)     : %s  (x^4 + x + 1)\n", GENERATOR);
    printf("Computed CRC       : %s\n", crc);
    printf("Frame (data + CRC) : %s\n", frame);

    if (corrupt) {
        int flip_pos = 3; /* flip a bit inside the data portion of the frame */
        printf("Corruption: flipping bit at position %d of the frame (%c -> %c)\n",
               flip_pos, frame[flip_pos], frame[flip_pos] == '0' ? '1' : '0');
        frame[flip_pos] = (frame[flip_pos] == '0') ? '1' : '0';
        printf("Corrupted frame    : %s\n", frame);
    }

    /* ---- send the frame to the receiver over TCP ---- */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        exit(1);
    }

    char out[320];
    snprintf(out, sizeof(out), "%s\n", frame); /* newline marks end of frame */
    send(sock, out, strlen(out), 0);

    printf("Frame sent to receiver over TCP (port %d).\n", PORT);

    close(sock);
    return 0;
}
