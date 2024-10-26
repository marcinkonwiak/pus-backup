#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int server_fd; // Moved out to be accessible in signal handler

float perform_operation(const float num1, const char operator, const float num2) {
    switch (operator) {
        case '+': return num1 + num2;
        case '-': return num1 - num2;
        case '*': return num1 * num2;
        case '/': return (num2 != 0) ? num1 / num2 : 0;
        default: return 0;
    }
}

// Signal handler for SIGINT
void handle_sigint(int sig) {
    printf("\nCaught signal %d, shutting down gracefully...\n", sig);
    close(server_fd);  // Close the server socket
    exit(0);           // Exit the program
}

int main() {
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Set up SIGINT signal handler
    signal(SIGINT, handle_sigint);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set server address properties
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the address and port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        // Loop to handle multiple requests from the same client
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            int valread = read(new_socket, buffer, BUFFER_SIZE);
            if (valread <= 0) {
                // Client disconnected or error occurred
                printf("Client disconnected\n");
                break;
            }

            // Process the request
            float num1, num2;
            char operator;
            sscanf(buffer, "%f %c %f", &num1, &operator, &num2);
            float result = perform_operation(num1, operator, num2);

            // Send the result back
            snprintf(buffer, BUFFER_SIZE, "%f", result);
            send(new_socket, buffer, strlen(buffer), 0);

            printf("%f %c %f = %f\n", num1, operator, num2, result);
        }

        // Close the connection after client disconnects
        close(new_socket);
    }

}
