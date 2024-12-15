#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#define MAX_IPS 100

char *allowed_ips[MAX_IPS + 1];
int allowed_ip_count = 0;

int load_ips_from_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp) && allowed_ip_count < MAX_IPS) {
        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0') {
            continue;
        }

        allowed_ips[allowed_ip_count] = strdup(line);
        if (!allowed_ips[allowed_ip_count]) {
            perror("strdup");
            fclose(fp);
            return -1;
        }
        allowed_ip_count++;
    }

    allowed_ips[allowed_ip_count] = NULL;
    fclose(fp);
    return 0;
}

int is_ip_allowed(const char *ip) {
    for (int i = 0; allowed_ips[i] != NULL; i++) {
        if (strcmp(ip, allowed_ips[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int main() {
    int sockfd;
    unsigned char buffer[65535];
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);

    if (load_ips_from_file("ip.txt") < 0) {
        fprintf(stderr, "Failed to read file.\n");
        exit(EXIT_FAILURE);
    }

    int server_port = 8079;

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    printf("Server running\n");

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

        struct in_addr src_ip;
        src_ip.s_addr = ip->saddr;
        if (!is_ip_allowed(inet_ntoa(src_ip))) {
            printf("Received packet from not allowed IP %s\n", inet_ntoa(src_ip));
            continue;
        }

        int payload_len = n - (ip_header_len + tcp_header_len);
        unsigned char *payload = (unsigned char *)(buffer + ip_header_len + tcp_header_len);

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
