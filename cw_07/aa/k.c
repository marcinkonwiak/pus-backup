#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <sys/types.h>

/*
 * Compute checksum for IP/UDP headers and data
 * For simplicity, we just show a basic IP checksum calculation function.
 */
unsigned short csum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
    for (; nwords > 0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

int main() {
    int sockfd;
    char buffer[4096]; // Enough size for IP + UDP headers + data
    struct ip *ip_hdr = (struct ip *) buffer;
    struct udphdr *udp_hdr = (struct udphdr *) (buffer + sizeof(struct ip));
    const char *data = "Fake IP test";
    int data_len = strlen(data);

    // Raw socket - requires root privileges
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // We tell the kernel we will provide IP header ourselves
    int one = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Zero out the buffer
    memset(buffer, 0, sizeof(buffer));

    // Setup addresses
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(2341); // destination port
    if (inet_pton(AF_INET, "192.168.1.100", &sin.sin_addr) <= 0) {
        perror("inet_pton destination");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Fill in IP Header
    ip_hdr->ip_hl = 5; // header length in 32-bit words
    ip_hdr->ip_v = 4;  // IPv4
    ip_hdr->ip_tos = 0;
    ip_hdr->ip_len = htons(sizeof(struct ip) + sizeof(struct udphdr) + data_len);
    ip_hdr->ip_id = htons(54321);
    ip_hdr->ip_off = 0;
    ip_hdr->ip_ttl = 64;
    ip_hdr->ip_p = IPPROTO_UDP;

    // FAKE SOURCE IP:
    if (inet_pton(AF_INET, "127.0.0.1", &ip_hdr->ip_src) <= 0) {
        perror("inet_pton source");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Destination IP:
    ip_hdr->ip_dst = sin.sin_addr;

    // IP checksum
    ip_hdr->ip_sum = 0;
    ip_hdr->ip_sum = csum((unsigned short *) buffer, sizeof(struct ip) / 2);

    // UDP Header
    udp_hdr->uh_sport = htons(9999);   // Fake source port
    udp_hdr->uh_dport = htons(2341);   // Destination port
    udp_hdr->uh_ulen = htons(sizeof(struct udphdr) + data_len);
    udp_hdr->uh_sum = 0; // For simplicity, we skip UDP checksum calc. (Should do it in real scenario)

    // Copy data
    memcpy(buffer + sizeof(struct ip) + sizeof(struct udphdr), data, data_len);

    // Send the packet
    if (sendto(sockfd, buffer, sizeof(struct ip) + sizeof(struct udphdr) + data_len, 0,
               (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        perror("sendto");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Packet sent with fake source IP.\n");

    close(sockfd);
    return 0;
}
