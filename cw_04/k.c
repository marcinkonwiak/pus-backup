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

void handle_exit(int sig) {
    printf("\nClosing\n");
    struct linger so_linger;
    so_linger.l_onoff = 1;
    so_linger.l_linger = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)) < 0) {
        perror("setsockopt failed");
    }

    close(sock);
    exit(0);
}

int main() {
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE] = {0};
    char message[100];
    fd_set readfds;
    int maxfd;
    int prompt_printed = 0;
    int stdineof = 0;

    signal(SIGINT, handle_exit);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nSocket creation error\n");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        printf("\nInvalid address\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printf("\nConnection Failed\n");
        return -1;
    }

    while (1) {
        if (!prompt_printed) {
            printf("Enter expression (e.g., 1 + 1): ");
            fflush(stdout);
            prompt_printed = 1;
        }

        FD_ZERO(&readfds);
        if (stdineof == 0) {
            FD_SET(0, &readfds);
        }
        FD_SET(sock, &readfds);

        maxfd = sock > 0 ? sock : 0;

        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            printf("Select error\n");
            break;
        }

        if (FD_ISSET(0, &readfds)) {
            // Input from stdin
            char input[256];
            if (fgets(input, sizeof(input), stdin) != NULL) {
                prompt_printed = 0; // Reset prompt flag
                // Remove trailing newline
                size_t len = strlen(input);
                if (len > 0 && input[len - 1] == '\n') {
                    input[len - 1] = '\0';
                }

                // Process the input
                float num1, num2;
                char operator;
                int i = sscanf(input, "%f %c %f", &num1, &operator, &num2);
                if (i == 3) {
                    snprintf(message, sizeof(message), "%f %c %f", num1, operator, num2);
                    send(sock, message, strlen(message), 0);
                } else if (i == -1) {
                    stdineof = 1;
                    shutdown(sock, SHUT_WR);
                    FD_CLR(0, &readfds);
                    continue;
                }
                else {
                    printf("Invalid input\n");
                    continue;
                }
            }
        }

        if (FD_ISSET(sock, &readfds)) {
            memset(buffer, 0, sizeof(buffer));
            int valread = read(sock, buffer, BUFFER_SIZE);
            if (valread > 0) {
                printf("\033[2K\n");
                printf("Result: %s\n", buffer);
                prompt_printed = 0;
            } else if (valread == 0) {
                printf("Server closed connection\n");
                break;
            } else {
                perror("Read error");
                break;
            }
        }
    }

    // Clean up
    close(sock);
    return 0;
}
