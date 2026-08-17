/*
 * udp_server.c
 * UDP server for Assignment 2, Part 1(b).
 * Reads student records from students.csv at startup, then waits for
 * roll-number datagrams on port 6061 and replies with just the roll
 * number and name (or "Student Record Not Found").
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 6061
#define MAX_STUDENTS 100
#define BUF_SIZE 512

typedef struct {
    char roll[32];
    char name[64];
    char dept[64];
    char sem[8];
    char cgpa[8];
} Student;

Student students[MAX_STUDENTS];
int student_count = 0;

void load_students(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("fopen students.csv");
        exit(1);
    }

    char line[256];
    fgets(line, sizeof(line), fp); /* skip the header row */

    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;

        Student s;
        char *token = strtok(line, ",");
        strncpy(s.roll, token, sizeof(s.roll) - 1); s.roll[sizeof(s.roll) - 1] = '\0';
        token = strtok(NULL, ","); strncpy(s.name, token, sizeof(s.name) - 1); s.name[sizeof(s.name) - 1] = '\0';
        token = strtok(NULL, ","); strncpy(s.dept, token, sizeof(s.dept) - 1); s.dept[sizeof(s.dept) - 1] = '\0';
        token = strtok(NULL, ","); strncpy(s.sem, token, sizeof(s.sem) - 1); s.sem[sizeof(s.sem) - 1] = '\0';
        token = strtok(NULL, ","); strncpy(s.cgpa, token, sizeof(s.cgpa) - 1); s.cgpa[sizeof(s.cgpa) - 1] = '\0';

        students[student_count++] = s;
    }

    fclose(fp);
    printf("Loaded %d student records from %s\n", student_count, filename);
}

int main() {
    load_students("students.csv");

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("UDP server listening on port %d...\n", PORT);
    fflush(stdout);

    char buf[BUF_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    /* UDP has no accept()/connection step - every datagram just arrives
     * here with the sender's address attached, so one loop handles all
     * clients one request at a time (no threads needed for this lab). */
    while (1) {
        int n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                          (struct sockaddr *)&client_addr, &client_len);
        if (n <= 0) continue;
        buf[n] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';

        printf("Query from %s:%d -> Roll Number %s\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buf);
        fflush(stdout);

        char response[BUF_SIZE];
        int found = 0;

        for (int i = 0; i < student_count; i++) {
            if (strcmp(students[i].roll, buf) == 0) {
                snprintf(response, sizeof(response), "Roll Number: %s\nName: %s\n",
                         students[i].roll, students[i].name);
                found = 1;
                break;
            }
        }

        if (!found) {
            snprintf(response, sizeof(response), "Student Record Not Found\n");
        }

        sendto(sock, response, strlen(response), 0,
               (struct sockaddr *)&client_addr, client_len);
    }

    close(sock);
    return 0;
}
