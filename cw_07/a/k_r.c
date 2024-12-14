#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <sys/time.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int sock = 0;       // Raw socket for sending
int sock_recv = 0;  // Normal UDP socket for receiving

// Compute checksum for IP/UDP headers
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
    if (sock > 0) close(sock);
    if (sock_recv > 0) close(sock_recv);
    exit(0);
}

int main() {
    char buffer[BUFFER_SIZE];
    char message[100];
    signal(SIGINT, handle_exit);

    // Create raw socket for sending
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

    // Create a normal UDP socket for receiving responses
    if ((sock_recv = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("UDP receive socket creation failed");
        close(sock);
        return -1;
    }

    struct sockaddr_in recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(54321); // same as we will use in the UDP header
    if (inet_pton(AF_INET, "127.0.0.1", &recv_addr.sin_addr) <= 0) {
        perror("Invalid fake IP address");
        close(sock);
        close(sock_recv);
        return -1;
    }

    int opt = 1;
    setsockopt(sock_recv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(sock_recv, (struct sockaddr *)&recv_addr, sizeof(recv_addr)) < 0) {
        perror("bind on receiving socket failed");
        close(sock);
        close(sock_recv);
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(sock_recv, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt for timeout failed");
        close(sock);
        close(sock_recv);
        return -1;
    }

    // Server address for sending (destination)
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
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

            // Spoofed source IP (must be something we can receive on, e.g. 127.0.0.1)
            if (inet_pton(AF_INET, "127.0.0.1", &ip_header->ip_src) <= 0) {
                printf("\nInvalid fake IP address\n");
                continue;
            }

            ip_header->ip_dst = server_address.sin_addr;

            // Calculate IP checksum
            ip_header->ip_sum = 0;
            ip_header->ip_sum = csum((unsigned short *)packet, sizeof(struct ip) / 2);

            // UDP header
            struct udphdr *udp_header = (struct udphdr *)(packet + sizeof(struct ip));
            udp_header->uh_sport = htons(54321);  // Fake source port (must match the bind above)
            udp_header->uh_dport = htons(PORT);    // Destination port
            udp_header->uh_ulen = htons(sizeof(struct udphdr) + strlen(message));
            udp_header->uh_sum = 0;  // UDP checksum (optional, can be 0)

            // Copy the message
            strcpy(packet + sizeof(struct ip) + sizeof(struct udphdr), message);

            // Send the packet
            ssize_t sent = sendto(sock, packet, sizeof(struct ip) + sizeof(struct udphdr) + strlen(message), 0,
                                  (struct sockaddr *)&server_address, sizeof(server_address));
            if (sent < 0) {
                perror("Error sending packet");
                continue;
            }

            printf("Sent spoofed packet: '%s'\n", message);

            // Receive the response
            memset(buffer, 0, BUFFER_SIZE);
            struct sockaddr_in from_addr;
            socklen_t from_len = sizeof(from_addr);

            ssize_t recv_len = recvfrom(sock_recv, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&from_addr, &from_len);
            if (recv_len < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    printf("No response.\n");
                    close(sock);
                    close(sock_recv);
                    return 0;
                } else {
                    perror("recvfrom error");
                }
            } else {
                buffer[recv_len] = '\0';
                printf("Received response: %s\n", buffer);
            }

        }
    }

    close(sock);
    close(sock_recv);
    return 0;
}
