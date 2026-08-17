/*
 * https_client.c
 *
 * Assignment 3 - Advanced (OPTIONAL) Program.
 * Secure HTTPS client using TCP sockets + OpenSSL.
 *
 *   - Asks the user for an HTTPS host (e.g. example.com or google.com)
 *   - Opens a plain TCP connection to that host on port 443
 *   - Wraps the TCP socket in an OpenSSL SSL/TLS session (handshake)
 *   - Verifies the server certificate (chain + hostname)
 *   - Sends a plain HTTP GET request over the encrypted channel
 *   - Reads and decrypts the response
 *   - Prints TCP connection time, SSL handshake time, round trip time
 *     (time to first response byte) and data transfer rate.
 *
 * Build:
 *   gcc -Wall -Wextra -O2 -o https_client https_client.c -lssl -lcrypto
 */

#define _GNU_SOURCE /* for memmem() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>

#define RECV_CHUNK 8192

static double elapsed_ms(struct timespec start, struct timespec end)
{
    double sec_part  = (double)(end.tv_sec - start.tv_sec) * 1000.0;
    double nsec_part = (double)(end.tv_nsec - start.tv_nsec) / 1000000.0;
    return sec_part + nsec_part;
}

int main(void)
{
    char host[256];

    printf("Enter HTTPS host (e.g. example.com): ");
    if (!fgets(host, sizeof(host), stdin)) {
        fprintf(stderr, "Failed to read host.\n");
        return 1;
    }
    host[strcspn(host, "\r\n")] = '\0';
    if (host[0] == '\0') {
        strcpy(host, "example.com");
    }

    /* ---- plain TCP connect to port 443 ---- */
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int gai_err = getaddrinfo(host, "443", &hints, &res);
    if (gai_err != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(gai_err));
        return 1;
    }

    struct timespec t_tcp_start, t_tcp_end;
    clock_gettime(CLOCK_MONOTONIC, &t_tcp_start);

    int sockfd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);

    if (sockfd == -1) {
        fprintf(stderr, "Could not connect to %s:443\n", host);
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t_tcp_end);
    printf("TCP connection established to %s:443\n", host);

    /* ---- set up OpenSSL and do the TLS handshake ---- */
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        fprintf(stderr, "SSL_CTX_new failed\n");
        close(sockfd);
        return 1;
    }
    /* Use the system's default trust store to verify the server cert. */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_set_default_verify_paths(ctx);

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    /* SNI: tells the server which hostname we want (needed by most
       HTTPS servers that host more than one site on the same IP). */
    SSL_set_tlsext_host_name(ssl, host);
    /* Also check that the certificate's name actually matches "host". */
    SSL_set1_host(ssl, host);

    struct timespec t_ssl_start, t_ssl_end;
    clock_gettime(CLOCK_MONOTONIC, &t_ssl_start);

    if (SSL_connect(ssl) != 1) {
        fprintf(stderr, "SSL handshake failed:\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sockfd);
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t_ssl_end);

    printf("SSL handshake complete. Protocol: %s  Cipher: %s\n",
           SSL_get_version(ssl), SSL_get_cipher(ssl));

    /* ---- certificate verification ---- */
    long verify_result = SSL_get_verify_result(ssl);
    X509 *cert = SSL_get1_peer_certificate(ssl);
    if (cert) {
        char subject[256];
        X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject));
        printf("Server certificate subject: %s\n", subject);
        X509_free(cert);
    } else {
        printf("Server did not present a certificate!\n");
    }
    printf("Certificate verification: %s\n",
           (verify_result == X509_V_OK) ? "OK (trusted chain + hostname match)"
                                         : X509_verify_cert_error_string(verify_result));

    /* ---- send the GET request over the encrypted channel ---- */
    char request[512];
    snprintf(request, sizeof(request),
              "GET / HTTP/1.1\r\n"
              "Host: %s\r\n"
              "Connection: close\r\n"
              "\r\n",
              host);

    struct timespec t_req_sent, t_first_byte, t_done;
    clock_gettime(CLOCK_MONOTONIC, &t_req_sent);

    if (SSL_write(ssl, request, (int)strlen(request)) <= 0) {
        fprintf(stderr, "SSL_write failed\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sockfd);
        return 1;
    }

    /* ---- read and decrypt the response ---- */
    size_t capacity = RECV_CHUNK * 4;
    size_t total = 0;
    char *buf = malloc(capacity);
    if (!buf) {
        fprintf(stderr, "Out of memory.\n");
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sockfd);
        return 1;
    }

    int got_first_byte = 0;
    int n;
    while ((n = SSL_read(ssl, buf + total, (int)(capacity - total))) > 0) {
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
                SSL_free(ssl);
                SSL_CTX_free(ctx);
                close(sockfd);
                return 1;
            }
            buf = nbuf;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t_done);

    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sockfd);

    if (!got_first_byte) {
        fprintf(stderr, "No data received from server.\n");
        free(buf);
        return 1;
    }

    /* ---- split headers/body, save body, print a status line ---- */
    char *header_end = memmem(buf, total, "\r\n\r\n", 4);
    size_t header_len = header_end ? (size_t)(header_end - buf) : total;
    size_t body_len = header_end ? (total - header_len - 4) : 0;
    const char *body = header_end ? header_end + 4 : NULL;

    printf("\n--- Response headers ---\n%.*s\n", (int)header_len, buf);

    if (body && body_len > 0) {
        FILE *fp = fopen("downloaded_https_page.html", "wb");
        if (fp) {
            fwrite(body, 1, body_len, fp);
            fclose(fp);
            printf("Saved decrypted body to downloaded_https_page.html\n");
        }
    }

    /* ---- performance analysis ---- */
    double tcp_ms  = elapsed_ms(t_tcp_start, t_tcp_end);
    double ssl_ms  = elapsed_ms(t_ssl_start, t_ssl_end);
    double rtt_ms  = elapsed_ms(t_req_sent, t_first_byte);   /* time to first byte */
    double xfer_ms = elapsed_ms(t_req_sent, t_done);         /* full response time */
    double rate_Bps = (xfer_ms > 0.0) ? (double)total / (xfer_ms / 1000.0) : 0.0;

    printf("\n--- Performance ---\n");
    printf("TCP connection time : %.3f ms\n", tcp_ms);
    printf("SSL handshake time  : %.3f ms\n", ssl_ms);
    printf("Round trip time     : %.3f ms (time to first response byte)\n", rtt_ms);
    printf("Total data received : %zu bytes\n", total);
    printf("Transfer time       : %.3f ms\n", xfer_ms);
    printf("Data transfer rate  : %.2f bytes/sec (%.2f KB/s)\n",
           rate_Bps, rate_Bps / 1024.0);

    free(buf);
    return 0;
}
