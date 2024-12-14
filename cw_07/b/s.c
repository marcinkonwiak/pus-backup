#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

int main() {
    int sockfd;
    unsigned char buffer[65535];
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);
    int server_port = 8080; // change to your desired port

    // Create a raw socket that receives TCP packets (IP level)
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    printf("Server running");

    // Receive packets in a loop
    while (1) {
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                             (struct sockaddr *)&src_addr, &addrlen);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("recvfrom");
            break;
        }

        if (n < (int)sizeof(struct iphdr)) {
            // Packet too short to contain an IP header
            continue;
        }

        struct iphdr *ip = (struct iphdr *)buffer;
        int ip_header_len = ip->ihl * 4;
        if (ip_header_len < 20 || ip_header_len > n) {
            continue;
        }

        if (ip->protocol != IPPROTO_TCP) {
            continue;
        }

        if (n < ip_header_len + (int)sizeof(struct tcphdr)) {
            continue;
        }

        struct tcphdr *tcp = (struct tcphdr *)(buffer + ip_header_len);
        int tcp_header_len = tcp->doff * 4;
        if (tcp_header_len < 20 || ip_header_len + tcp_header_len > n) {
            continue;
        }

        if (ntohs(tcp->dest) != server_port) {
            continue;
        }

        // Calculate payload length
        int payload_len = n - (ip_header_len + tcp_header_len);
        unsigned char *payload = (unsigned char *)(buffer + ip_header_len + tcp_header_len);

        // Print source info and payload
        struct in_addr saddr;
        saddr.s_addr = ip->saddr;

        printf("Received packet from %s:%d\n", inet_ntoa(saddr), ntohs(tcp->source));

        if (payload_len > 0) {
            printf("received data: %.*s\n", payload_len, payload);
        } else {
            printf("No payload.\n");
        }
    }

    close(sockfd);
    return 0;
}
