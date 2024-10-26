#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int sock = 0;

void handle_exit(int sig) {
    printf("\nClosing connection...\n");
    close(sock);
    exit(0);
}

float get_float_input(const char *prompt) {
    char input[100];
    float value;
    char *endptr;

    while (1) {
        printf("%s", prompt);
        if (fgets(input, sizeof(input), stdin) != NULL) {
            errno = 0; // Reset errno before conversion
            value = strtof(input, &endptr);

            if (errno == 0 && endptr != input && *endptr == '\n') {
                return value; // Successful conversion
            } else {
                printf("Invalid input. Please enter a valid number.\n");
            }
        } else {
            printf("Error reading input.\n");
        }
    }
}

char get_operator_input() {
    char input[100];
    while (1) {
        printf("Enter operator (+, -, *, /): ");
        if (fgets(input, sizeof(input), stdin) != NULL && strlen(input) == 2 &&
            (input[0] == '+' || input[0] == '-' || input[0] == '*' || input[0] == '/')) {
            return input[0];
            } else {
                printf("Invalid operator. Please enter one of +, -, *, or /.\n");
            }
    }
}

int main() {
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE] = {0};
    char message[100];

    // Handle Ctrl+C to cleanly close the connection
    signal(SIGINT, handle_exit);

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nSocket creation error\n");
        return -1;
    }

    // Server address setup
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported\n");
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        printf("\nConnection Failed\n");
        return -1;
    }

    // Run the client in a loop
    while (1) {
        float num1 = get_float_input("Enter first number: ");
        char operator = get_operator_input();
        float num2 = get_float_input("Enter second number: ");

        // Prepare message
        snprintf(message, sizeof(message), "%f %c %f", num1, operator, num2);

        // Send message to server
        send(sock, message, strlen(message), 0);

        // Clear buffer and receive the response from server
        memset(buffer, 0, sizeof(buffer));
        read(sock, buffer, BUFFER_SIZE);
        printf("Result: %s\n", buffer);
    }
}
