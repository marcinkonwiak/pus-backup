#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int sock = 0;

void handle_exit(int sig) {
    printf("\nClosing\n");
    close(sock);
    exit(0);
}

int main() {
    signal(SIGINT, handle_exit);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    int broadcastPermission = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastPermission,
                   sizeof(broadcastPermission)) < 0) {
        perror("setsockopt(SO_BROADCAST) failed");
        close(sock);
        return -1;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.255.255", &server_address.sin_addr) <= 0) {
        printf("\nInvalid server address\n");
        return -1;
    }

    char *message = "1";

    char packet[BUFFER_SIZE];
    memset(packet, 0, BUFFER_SIZE);

    strcpy(packet + sizeof(struct ip) + sizeof(struct udphdr), message);

    if (sendto(sock, packet,
               sizeof(struct ip) + sizeof(struct udphdr) + strlen(message), 0,
               (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        close(sock);
        return 0;
    }

    printf("Sent: '%s'\n", message);

    close(sock);
    return 0;
}
