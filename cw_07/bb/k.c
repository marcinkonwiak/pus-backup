#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFFER_SIZE 1024
#define MY_PROTOCOL 253

int sock = 0;
int recv_sock = 0;

void handle_exit(int sig) {
    printf("\nClosing\n");
    close(sock);
    close(recv_sock);
    exit(0);
}

float get_float_input() {
    char input[100];
    float value;
    char *endptr;

    while (1) {
        if (fgets(input, sizeof(input), stdin) != NULL) {
            errno = 0;
            value = strtof(input, &endptr);
            if (errno == 0 && endptr != input && (*endptr == '\n' || *endptr == '\0')) {
                return value;
            } else {
                printf("Invalid input:\n");
            }
        } else {
            printf("Error reading input.\n");
        }
    }
}

char get_operator_input() {
    char input[100];
    while (1) {
        if (fgets(input, sizeof(input), stdin) != NULL && strlen(input) == 2 &&
            (input[0] == '+' || input[0] == '-' || input[0] == '*' || input[0] == '/')) {
            return input[0];
        }
        printf("Invalid operator\n");
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_exit);

    // Create a raw socket for sending
    if ((sock = socket(AF_INET, SOCK_RAW, MY_PROTOCOL)) < 0) {
        perror("socket");
        exit(1);
    }

    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt");
        exit(1);
    }

    // Also create a raw socket for receiving replies
    if ((recv_sock = socket(AF_INET, SOCK_RAW, MY_PROTOCOL)) < 0) {
        perror("recv socket");
        exit(1);
    }

    struct in_addr src_ip, dst_ip;
    // Fake source IP
    if (inet_pton(AF_INET, "127.0.0.1", &src_ip) != 1) {
        perror("inet_pton src");
        exit(1);
    }
    // Destination (server) IP
    if (inet_pton(AF_INET, "127.0.0.1", &dst_ip) != 1) {
        perror("inet_pton dst");
        exit(1);
    }

    // Prepare addresses
    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr = dst_ip;

    while (1) {
        printf("First number: ");
        float num1 = get_float_input();
        printf("Enter operator (+, -, *, /): ");
        char operator = get_operator_input();
        printf("Second number: ");
        float num2 = get_float_input();

        char payload[64];
        snprintf(payload, sizeof(payload), "%f %c %f", num1, operator, num2);

        unsigned char sendbuf[BUFFER_SIZE];
        memset(sendbuf, 0, BUFFER_SIZE);

        struct ip *ip_hdr = (struct ip*)sendbuf;
        ip_hdr->ip_hl = 5;
        ip_hdr->ip_v = 4;
        ip_hdr->ip_tos = 0;
        ip_hdr->ip_len = htons(sizeof(struct ip) + strlen(payload)+1);
        ip_hdr->ip_id = htons(54321);
        ip_hdr->ip_off = 0;
        ip_hdr->ip_ttl = 64;
        ip_hdr->ip_p = MY_PROTOCOL;
        ip_hdr->ip_src = src_ip;
        ip_hdr->ip_dst = dst_ip;

        memcpy(sendbuf + sizeof(struct ip), payload, strlen(payload)+1);

        // Compute IP checksum
        unsigned short *ip_header_words = (unsigned short*)ip_hdr;
        unsigned long sum = 0;
        for (int i=0; i<10; i++) sum += ip_header_words[i];
        while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
        ip_hdr->ip_sum = ~sum;

        // Send the packet
        if (sendto(sock, sendbuf, sizeof(struct ip) + strlen(payload)+1, 0,
                   (struct sockaddr*)&dst, sizeof(dst)) < 0) {
            perror("sendto");
            continue;
        }
        for (;;) {
            // Now wait for response
            unsigned char recvbuf[BUFFER_SIZE];
            struct sockaddr_in addr;
            socklen_t addrlen = sizeof(addr);
            ssize_t n = recvfrom(recv_sock, recvbuf, BUFFER_SIZE, 0, (struct sockaddr*)&addr, &addrlen);
            if (n <= 0) {
                perror("recvfrom");
                continue;
            }

            struct ip *riph = (struct ip*)recvbuf;
            unsigned char *reply_payload = recvbuf + (riph->ip_hl * 4);
            int reply_len = n - (riph->ip_hl * 4);

            if (reply_len > 0) {
                printf("Result: %.*s\n", reply_len, reply_payload);
            } else {
                printf("No reply payload received.\n");
            }
        }
    }

    close(sock);
    close(recv_sock);
    return 0;
}
