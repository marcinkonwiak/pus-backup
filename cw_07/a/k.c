#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int sock = 0;
struct sockaddr_in server_address;

void handle_exit(int sig) {
    printf("\nClosing\n");
    close(sock);
    exit(0);
}

int main() {
    char buffer[BUFFER_SIZE] = {0};
    char message[100];
    fd_set readfds;
    int maxfd;
    int prompt_printed = 0;
    int stdineof = 0;

    struct timeval tv;
    signal(SIGINT, handle_exit);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("\nSocket creation error\n");
        return -1;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        printf("\nInvalid address\n");
        return -1;
    }


    while (1) {
    if (!prompt_printed) {
        printf("Enter expression (e.g. 1 + 1): ");
        fflush(stdout);
        prompt_printed = 1;
    }

    FD_ZERO(&readfds);
    if (stdineof == 0) {
        FD_SET(STDIN_FILENO, &readfds);
    }
    FD_SET(sock, &readfds);

    maxfd = sock > STDIN_FILENO ? sock : STDIN_FILENO;

    struct timeval timeout;
    timeout.tv_sec = 5; 
    timeout.tv_usec = 0;

    int activity = select(maxfd + 1, &readfds, NULL, NULL, &timeout);

    if (activity < 0 && errno != EINTR) {
        printf("Select error\n");
        break;
    }

    if (activity == 0) {
        printf("\nServer closed\n");
        prompt_printed = 0;
        exit(0);
    }

    if (FD_ISSET(STDIN_FILENO, &readfds)) {
        char input[256];
        if (fgets(input, sizeof(input), stdin) != NULL) {
            prompt_printed = 0;
            size_t len = strlen(input);
            if (len > 0 && input[len - 1] == '\n') {
                input[len - 1] = '\0';
            }

            float num1, num2;
            char operator;
            int i = sscanf(input, "%f %c %f", &num1, &operator, &num2);
            if (i == 3) {
                snprintf(message, sizeof(message), "%f %c %f", num1, operator, num2);
                if (sendto(sock, message, strlen(message), 0, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
                    perror("Error sending data to server");
                    continue;
                }
            } else if (i == -1) {
                break;
            } else {
                printf("Invalid input\n");
                continue;
            }
        } else {
            stdineof = 1;
        }
    }

    if (FD_ISSET(sock, &readfds)) {
        memset(buffer, 0, sizeof(buffer));
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        int valread = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&from, &fromlen);
        if (valread > 0) {
            printf("\033[2K\n");
            printf("Result: %s\n", buffer);
            prompt_printed = 0;
        } else if (valread == 0) {
            printf("Server closed connection or no data\n");
            break;
        } else {
            perror("Error receiving data from server");
            break;
        }
    }
}

    close(sock);
    return 0;
}
