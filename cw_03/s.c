#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int server_fd;

float calculate(const float num1, const char operator, const float num2) {
    switch (operator) {
        case '+': return num1 + num2;
        case '-': return num1 - num2;
        case '*': return num1 * num2;
        case '/': return (num2 != 0) ? num1 / num2 : 0;
        default: return 0;
    }
}

void handle_client(int conn_fd) {
    char buffer[BUFFER_SIZE] = {0};
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int valread = read(conn_fd, buffer, BUFFER_SIZE);
        if (valread <= 0) {
            printf("Client disconnected\n");
            break;
        }

        float num1, num2;
        char operator;
        sscanf(buffer, "%f %c %f", &num1, &operator, &num2);
        float result = calculate(num1, operator, num2);

        snprintf(buffer, BUFFER_SIZE, "%f", result);
        send(conn_fd, buffer, strlen(buffer), 0);

        printf("%f %c %f = %f\n", num1, operator, num2, result);
    }
}

void handle_sigint(int sig) {
    printf("\nClosing server\n");
    close(server_fd);
    exit(0);
}

void sig_chld(int signo) {
    int stat;
    while (waitpid(-1, &stat, WNOHANG) > 0) {
        printf("Potomek zakonczony\n");
    }
}

int main() {
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    char buffer[BUFFER_SIZE] = {0};
    int conn_fd;

    signal(SIGINT, handle_sigint);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, sig_chld);

    printf("Server running on port %d...\n", PORT);

    while (1) {
        if ((conn_fd = accept(server_fd, (struct sockaddr *) &address,
                              (socklen_t *) &addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        // handle_client(conn_fd);
        if (fork() == 0) {
            close(server_fd);
            handle_client(conn_fd);
            close(conn_fd);
            exit(0);
        }

        close(conn_fd);
    }
}
