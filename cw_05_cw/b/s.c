#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 2341
#define BUFFER_SIZE 1024

int sockfd;

void handle_exit(int sig) {
    printf("\nClosing server\n");
    if (sockfd >= 0) {
        close(sockfd);
    }
    exit(0);
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;
    char buffer[BUFFER_SIZE];

    signal(SIGINT, handle_exit);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP Server listening for broadcasts on port %d...\n", PORT);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        addr_len = sizeof(client_addr);

        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &addr_len);
        if (n < 0) {
            perror("recvfrom error");
            continue;
        }

        buffer[n] = '\0';
        printf("Received broadcast message: '%s' from %s:%d\n",
            buffer,
            inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port)
        );

        const char *response = "2";
        if (sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&client_addr, addr_len) < 0) {
            perror("sendto error");
        }
    }

    close(sockfd);
    return 0;
}
