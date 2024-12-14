#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024
#define MY_PROTOCOL 253  // Experimental/Unassigned protocol for demo
#define LISTEN_PORT 0    // Not really used in raw packets, but can be a hint

int server_fd;

float calculate(const float num1, const char operator, const float num2) {
    switch (operator) {
        case '+': return num1 + num2;
        case '-': return num1 - num2;
        case '*': return num1 * num2;
        case '/': return (num2 != 0) ? num1 / num2 : 0;
        default: return 0;
    }
}

void handle_sigint(int sig) {
    printf("\nClosing server\n");
    close(server_fd);
    exit(0);
}

int main() {
    // Create a raw socket to listen for IP packets with our custom protocol
    if ((server_fd = socket(AF_INET, SOCK_RAW, MY_PROTOCOL)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, handle_sigint);

    printf("Server running on raw socket protocol %d...\n", MY_PROTOCOL);

    // Buffer to store incoming packets
    unsigned char buffer[BUFFER_SIZE];
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    for (;;) {
        memset(&cli_addr, 0, sizeof(cli_addr));
        memset(buffer, 0, BUFFER_SIZE);

        // Receive raw packet
        ssize_t n = recvfrom(server_fd, buffer, BUFFER_SIZE, 0,
                             (struct sockaddr *)&cli_addr, &cli_len);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            perror("recvfrom");
            break;
        }

        // IP header is at the start
        struct ip *iph = (struct ip*)buffer;
        // Payload starts after IP header
        unsigned char *payload = buffer + (iph->ip_hl * 4);
        int payload_len = n - (iph->ip_hl * 4);

        if (payload_len <= 0) {
            continue;
        }

        // Parse the payload: "num1 operator num2"
        float num1, num2;
        char operator;
        int items = sscanf((char*)payload, "%f %c %f", &num1, &operator, &num2);
        if (items != 3) {
            fprintf(stderr, "Received malformed request\n");
            continue;
        }

        float result = calculate(num1, operator, num2);
        printf("Received: %f %c %f = %f\n", num1, operator, num2, result);

        // Now send response back
        // We'll send a raw IP packet back to the client.
        // The client's IP is in iph->ip_src. We'll reverse source/dest.

        int sock_fd = socket(AF_INET, SOCK_RAW, MY_PROTOCOL);
        if (sock_fd < 0) {
            perror("socket for send");
            continue;
        }

        int one = 1;
        if (setsockopt(sock_fd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
            perror("setsockopt IP_HDRINCL");
            close(sock_fd);
            continue;
        }

        // Construct IP packet
        unsigned char sendbuf[BUFFER_SIZE];
        memset(sendbuf, 0, BUFFER_SIZE);
        struct ip *ip_out = (struct ip*)sendbuf;
        ip_out->ip_hl = 5;
        ip_out->ip_v = 4;
        ip_out->ip_tos = 0;
        ip_out->ip_len = htons(sizeof(struct ip) + 64);
        ip_out->ip_id = htons(54321);
        ip_out->ip_off = 0;
        ip_out->ip_ttl = 64;
        ip_out->ip_p = MY_PROTOCOL;
        ip_out->ip_src = iph->ip_dst; // Our IP
        ip_out->ip_dst = iph->ip_src; // Client's IP

        // Fill response payload with result
        snprintf((char*)(sendbuf + sizeof(struct ip)), 64, "%f", result);

        // Compute IP checksum
        // Simple checksum function
        unsigned short *ip_header_words = (unsigned short*)ip_out;
        unsigned long sum = 0;
        for (int i=0; i < 10; i++) sum += ip_header_words[i];
        while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
        ip_out->ip_sum = ~sum;

        // Destination for sendto
        struct sockaddr_in dst;
        memset(&dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        dst.sin_addr = iph->ip_src;

        if (sendto(sock_fd, sendbuf, sizeof(struct ip) + 64, 0,
                   (struct sockaddr *)&dst, sizeof(dst)) < 0) {
            perror("sendto");
        }

        close(sock_fd);
    }

    close(server_fd);
    return 0;
}
