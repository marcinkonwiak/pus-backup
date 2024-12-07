#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>

#define PORT 2341
#define BUFFER_SIZE 1024

int sockfd;

void handle_exit(int sig) {
    printf("\nClosing client\n");
    if (sockfd >= 0) {
        close(sockfd);
    }
    exit(0);
}

int main() {
    struct sockaddr_in broadcast_addr, from;
    char buffer[BUFFER_SIZE];
    fd_set readfds;
    int maxfd;
    struct timeval tv;

    signal(SIGINT, handle_exit);

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Enable broadcast option
    int broadcastPermission = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastPermission, sizeof(broadcastPermission)) < 0) {
        perror("setsockopt(SO_BROADCAST) failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(PORT);
    // For a broadcast, we typically use the limited broadcast address:
    // broadcast_addr.sin_addr.s_addr = inet_addr("localhost");

    // Broadcast a message
    const char *message = "1";
    if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
    perror("sendto failed");
    close(sockfd);
    exit(EXIT_FAILURE);
    }

    // Wait for a response
    // We'll use select with a timeout so we don't block indefinitely
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    maxfd = sockfd;

    tv.tv_sec = 5;  // Wait up to 5 seconds
    tv.tv_usec = 0;

    printf("Broadcast query sent. Waiting for replies...\n");

    int activity = select(maxfd + 1, &readfds, NULL, NULL, &tv);

    if (activity < 0 && errno != EINTR) {
        perror("select error");
        close(sockfd);
        exit(EXIT_FAILURE);
    } else if (activity == 0) {
        printf("No response from any server.\n");
    } else {
        if (FD_ISSET(sockfd, &readfds)) {
            memset(buffer, 0, sizeof(buffer));
            socklen_t fromlen = sizeof(from);
            int valread = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&from, &fromlen);
            if (valread > 0) {
                buffer[valread] = '\0';
                printf("Received response from server at %s: %s\n", inet_ntoa(from.sin_addr), buffer);
            } else if (valread == 0) {
                printf("No data received.\n");
            } else {
                perror("recvfrom error");
            }
        }
    }

    close(sockfd);
    return 0;
}
