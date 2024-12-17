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

unsigned short csum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
    for (; nwords > 0; nwords--) {
        sum += *buf++;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

void handle_exit(int sig) {
    printf("\nClosing\n");
    close(sock);
    exit(0);
}

int main() {
    char buffer[BUFFER_SIZE];
    char message[100];
    signal(SIGINT, handle_exit);

    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt error");
        close(sock);
        return -1;
    }

    int broadcastPermission = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastPermission, sizeof(broadcastPermission)) < 0) {
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

    while (1) {
        printf("Enter expression (e.g. 1 + 1): ");
        if (fgets(message, sizeof(message), stdin) != NULL) {
            size_t len = strlen(message);
            if (len > 0 && message[len - 1] == '\n') {
                message[len - 1] = '\0';
            }

            char packet[BUFFER_SIZE];
            memset(packet, 0, BUFFER_SIZE);

            // IP header
            struct ip *ip_header = (struct ip *)packet;
            ip_header->ip_hl = 5;  // Header length
            ip_header->ip_v = 4;   // IPv4
            ip_header->ip_tos = 0;
            ip_header->ip_len = htons(sizeof(struct ip) + sizeof(struct udphdr) + strlen(message));
            ip_header->ip_id = htonl(54321);
            ip_header->ip_off = 0;
            ip_header->ip_ttl = 64;
            ip_header->ip_p = IPPROTO_UDP;

            // Spoofed source IP
            if (inet_pton(AF_INET, "12.13.14.15", &ip_header->ip_src) <= 0) {
                printf("\nInvalid IP address\n");
                continue;
            }

            // Server IP
            ip_header->ip_dst = server_address.sin_addr;

            ip_header->ip_sum = 0;
            ip_header->ip_sum = csum((unsigned short *)packet, sizeof(struct ip) / 2);

            // UDP header
            struct udphdr *udp_header = (struct udphdr *)(packet + sizeof(struct ip));
            udp_header->uh_sport = htons(54321);
            udp_header->uh_dport = htons(PORT);
            udp_header->uh_ulen = htons(sizeof(struct udphdr) + strlen(message));
            udp_header->uh_sum = 0;

            strcpy(packet + sizeof(struct ip) + sizeof(struct udphdr), message);

            if (sendto(sock, packet, sizeof(struct ip) + sizeof(struct udphdr) + strlen(message), 0,
                       (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
                perror("Error sending");
                continue;
            }

            printf("Sent: '%s'\n", message);
        }
    }

    close(sock);
    return 0;
}
