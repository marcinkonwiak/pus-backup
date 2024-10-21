#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int sock = 0;

void handle_exit(int sig) {
    printf("\nClosing connection...\n");
    close(sock);
    exit(0);
}

int main() {
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE] = {0};
    char message[100];
    float num1, num2;
    char operator;

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
        // Input numbers and operator
        printf("Enter first number: ");
        scanf("%f", &num1);
        printf("Enter second number: ");
        scanf("%f", &num2);
        printf("Enter operator (+, -, *, /): ");
        scanf(" %c", &operator);

        // Prepare message
        snprintf(message, sizeof(message), "%f %c %f", num1, operator, num2);

        // Send message to server
        send(sock, message, strlen(message), 0);

        // Clear buffer and receive the response from server
        memset(buffer, 0, sizeof(buffer));
        int valread = read(sock, buffer, BUFFER_SIZE);
        printf("Result: %s\n", buffer);
    }

    return 0;
}
