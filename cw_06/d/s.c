#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <syslog.h>
#include <netdb.h>

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

void handle_sigint(int sig) {
    printf("\nClosing server\n");
    syslog(LOG_INFO, "UDP server shutting down.");
    close(server_fd);
    closelog();
    exit(0);
}

int main() {
    struct sockaddr_in address, cliaddr;
    socklen_t clilen;
    char buffer[BUFFER_SIZE] = {0};

    openlog("udp_server", LOG_PID | LOG_CONS, LOG_USER);

    signal(SIGINT, handle_sigint);

    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        syslog(LOG_ERR, "Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        syslog(LOG_ERR, "setsockopt failed.");
        close(server_fd);
        closelog();
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        syslog(LOG_ERR, "Bind failed.");
        close(server_fd);
        closelog();
        exit(EXIT_FAILURE);
    }

    printf("UDP Server running on port %d...\n", PORT);
    syslog(LOG_INFO, "UDP server started on port %d.", PORT);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        clilen = sizeof(cliaddr);
        int n = recvfrom(server_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &clilen);
        if (n < 0) {
            perror("recvfrom failed");
            syslog(LOG_ERR, "recvfrom failed.");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cliaddr.sin_addr, client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Received request from %s:%d", client_ip, ntohs(cliaddr.sin_port));

        float num1, num2;
        char operator;
        if (sscanf(buffer, "%f %c %f", &num1, &operator, &num2) == 3) {
            float result = calculate(num1, operator, num2);
            snprintf(buffer, BUFFER_SIZE, "%f", result);
            sendto(server_fd, buffer, strlen(buffer), 0, (struct sockaddr *)&cliaddr, clilen);
            printf("%f %c %f = %f\n", num1, operator, num2, result);
        } else {
            // Invalid input format, send an error response
            char *err_msg = "Invalid input.";
            sendto(server_fd, err_msg, strlen(err_msg), 0, (struct sockaddr *)&cliaddr, clilen);
        }
    }

    close(server_fd);
    closelog();
    return 0;
}
