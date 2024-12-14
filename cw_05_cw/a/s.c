#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

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
    close(server_fd);
    exit(0);
}

int main() {
    struct sockaddr_in address, cliaddr;
    socklen_t clilen;
    char buffer[BUFFER_SIZE] = {0};

    signal(SIGINT, handle_sigint);

    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("UDP Server running on port %d...\n", PORT);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        clilen = sizeof(cliaddr);
        int n = recvfrom(server_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &clilen);
        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }

        float num1, num2;
        char operator;
        if (sscanf(buffer, "%f %c %f", &num1, &operator, &num2) == 3) {
            float result = calculate(num1, operator, num2);
            snprintf(buffer, BUFFER_SIZE, "%f", result);
            sendto(server_fd, buffer, strlen(buffer), 0, (struct sockaddr *)&cliaddr, clilen);
            printf("%f %c %f = %f\n", num1, operator, num2, result);
        } else {
            char *err_msg = "Invalid input.";
            sendto(server_fd, err_msg, strlen(err_msg), 0, (struct sockaddr *)&cliaddr, clilen);
        }
    }

    close(server_fd);
    return 0;
}
