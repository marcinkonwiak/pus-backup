#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // For close()
#include <string.h>     // For memset()
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>     // For signal handling
#include <errno.h>      // For errno

#define PORT 8080

volatile sig_atomic_t stop = 0;

void intHandler(int dummy) {
    stop = 1;
}

int main() {
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    char buffer[256];
    int n;

    // Set up the signal handler
    signal(SIGINT, intHandler);

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    // Initialize socket structure
    memset((char *) &serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;         // IPv4
    serv_addr.sin_addr.s_addr = INADDR_ANY; // Any IP address
    serv_addr.sin_port = htons(PORT);       // Port number

    // Bind the host address
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        close(sockfd);
        exit(1);
    }

    // Start listening for the clients
    listen(sockfd, 5);
    printf("Server listening on port %d\n", PORT);

    while (!stop) {
        // Accept actual connection from the client
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd,
                           (struct sockaddr *) &cli_addr,
                           &clilen);
        if (newsockfd < 0) {
            if (errno == EINTR) {
                // Interrupted by SIGINT
                if (stop) {
                    printf("\nShutting down server gracefully...\n");
                    break;
                } else {
                    continue;
                }
            } else {
                perror("ERROR on accept");
                break;
            }
        }

        printf("Connected to client: %s\n",
               inet_ntoa(cli_addr.sin_addr));

        // Read the message from the client
        memset(buffer, 0, 256);
        n = read(newsockfd, buffer, 255);
        if (n < 0) {
            perror("ERROR reading from socket");
            close(newsockfd);
            continue;
        }

        printf("Received message: %s\n", buffer);

        float num1, num2;
        char op;
        // Parse the received message
        if (sscanf(buffer, "%f %c %f", &num1, &op, &num2) != 3) {
            char *msg = "Invalid input format. Expected format: number1 operator number2\n";
            write(newsockfd, msg, strlen(msg));
            close(newsockfd);
            continue;
        }

        float result;
        char *error_msg = NULL;

        // Perform the arithmetic operation
        switch(op) {
            case '+':
                result = num1 + num2;
                break;
            case '-':
                result = num1 - num2;
                break;
            case '*':
                result = num1 * num2;
                break;
            case '/':
                if (num2 == 0) {
                    error_msg = "Error: Division by zero\n";
                } else {
                    result = num1 / num2;
                }
                break;
            default:
                error_msg = "Invalid operator. Supported operators are +, -, *, /\n";
                break;
        }

        if (error_msg) {
            write(newsockfd, error_msg, strlen(error_msg));
            close(newsockfd);
            continue;
        }

        // Send back the result to the client
        char result_str[256];
        snprintf(result_str, sizeof(result_str), "Result: %f\n", result);
        n = write(newsockfd, result_str, strlen(result_str));
        if (n < 0) {
            perror("ERROR writing to socket");
        }

        // Close the connection
        close(newsockfd);
    }

    // Close the server socket
    close(sockfd);
    printf("Server has been shut down.\n");
    return 0;
}
