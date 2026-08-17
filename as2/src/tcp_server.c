/*
 * tcp_server.c
 * TCP server for Assignment 2, Part 1(a).
 * Reads student records from students.csv at startup, then listens on
 * port 6060. Every accepted client connection is handled on its own
 * pthread so multiple clients can be served concurrently.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 6060
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

/* Loads all student rows from the CSV into the students[] array.
 * The array is filled once at startup and only ever read afterwards,
 * so the worker threads can safely search it without any locking. */
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

/* Runs in its own thread per connected client. */
void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buf[BUF_SIZE];
    int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return NULL;
    }
    buf[n] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';

    char response[BUF_SIZE];
    int found = 0;

    for (int i = 0; i < student_count; i++) {
        if (strcmp(students[i].roll, buf) == 0) {
            snprintf(response, sizeof(response),
                "Roll Number: %s\nName: %s\nDepartment: %s\nSemester: %s\nCGPA: %s\n",
                students[i].roll, students[i].name, students[i].dept,
                students[i].sem, students[i].cgpa);
            found = 1;
            break;
        }
    }

    if (!found) {
        snprintf(response, sizeof(response), "Student Record Not Found\n");
    }

    send(client_fd, response, strlen(response), 0);
    close(client_fd);
    return NULL;
}

int main() {
    load_students("students.csv");

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

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

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(1);
    }

    printf("TCP server listening on port %d...\n", PORT);
    fflush(stdout);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (*client_fd < 0) {
            perror("accept");
            free(client_fd);
            continue;
        }

        printf("Client connected: %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        fflush(stdout);

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid); /* thread cleans itself up when done, no need to join */
    }

    close(server_fd);
    return 0;
}
