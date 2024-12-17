#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#define PORT 8085
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


char get_operator_input() {
    char input[100];
    while (1) {
        if (fgets(input, sizeof(input), stdin) != NULL && strlen(input) == 2 &&
            (input[0] == '+' || input[0] == '-' || input[0] == '*' || input[0] ==
             '/')) {
            return input[0];
        }
        printf("Invalid operator\n");
    }
}

int main() {
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE] = {0};
    char message[100];

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

    if (connect(sock, (struct sockaddr *) &server_address,
                sizeof(server_address)) < 0) {
        printf("\nConnection Failed\n");
        return -1;
    }

    float num1 = 1;
    char operator = '+';
    float num2 = 2;

    snprintf(message, sizeof(message), "%f %c %f", num1, operator, num2);
    send(sock, message, strlen(message), 0);

    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, BUFFER_SIZE);
    printf("Result: %s\n", buffer);
}
