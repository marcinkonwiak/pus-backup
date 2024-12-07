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
        printf("Client disconnected\n");
    }
}

int main() {
    struct sockaddr_in address, cliaddr;
    socklen_t clilen;
    int addrlen = sizeof(address);
    int LISTENQ = 10;
    int client[FD_SETSIZE];
    int nready;
    fd_set rset, allset;
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
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, LISTENQ) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    int maxfd = server_fd;
    int maxi = -1;
    for (int i = 0; i < FD_SETSIZE; i++) {
        client[i] = -1;
    }
    FD_ZERO(&allset);
    FD_SET(server_fd, &allset);

    signal(SIGCHLD, sig_chld);

    printf("Server running on port %d...\n", PORT);

    while (1) {
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &rset)) {
            clilen = sizeof(cliaddr);
            conn_fd = accept(server_fd, (struct sockaddr *) &cliaddr, &clilen);
            printf("New client connected\n");

            int i;
            for (i = 0; i < FD_SETSIZE; i++) {
                if (client[i] < 0) {
                    client[i] = conn_fd;
                    break;
                }
            }

            if (i == FD_SETSIZE) {
                perror("Too many clients");
                exit(EXIT_FAILURE);
            }

            FD_SET(conn_fd, &allset);
            if (conn_fd > maxfd) {
                maxfd = conn_fd;
            }

            if (i > maxi) {
                maxi = i;
            }

            if (--nready <= 0) {
                continue;
            }
        }

        for (int i = 0; i <= maxi; i++) {
            int sockfd = client[i];
            if (sockfd < 0) continue;

            if (FD_ISSET(sockfd, &rset)) {
                char buffer[BUFFER_SIZE] = {0};
                int n = read(sockfd, buffer, BUFFER_SIZE);
                if (n <= 0) {
                    
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i] = -1;
                } else {
                    float num1, num2;
                    char operator;
                    sscanf(buffer, "%f %c %f", &num1, &operator, &num2);
                    float result = calculate(num1, operator, num2);
                    snprintf(buffer, BUFFER_SIZE, "%f", result);
                    send(sockfd, buffer, strlen(buffer), 0);

                    printf("%f %c %f = %f\n", num1, operator, num2, result);
                }

                if (--nready <= 0) {
                    break;
                }
            }
        }
    }
}
