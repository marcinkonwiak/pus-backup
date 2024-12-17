#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#define PORT 8085
#define BUFFER_SIZE 1024
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
    int server_fd;
    char buffer[4096];
    struct sockaddr_in saddr;
    socklen_t saddr_len = sizeof(saddr);

    if (load_ips_from_file("ip.txt") < 0) {
        fprintf(stderr, "Failed to load IP addresses from file.\n");
        exit(EXIT_FAILURE);
    }

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(PORT);

    server_fd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    if (bind(server_fd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Server running...\n");

    while (1) {
        ssize_t bytes = recvfrom(server_fd, buffer, sizeof(buffer), 0,
                                 (struct sockaddr*)&saddr, &saddr_len);
        if (bytes < 0) {
            perror("recvfrom");
            continue;
        }

        struct iphdr *ip_header = (struct iphdr *)buffer;
        int ip_header_len = ip_header->ihl * 4;
        struct tcphdr *tcp_header = (struct tcphdr *)(buffer + ip_header_len);

        if (ntohs(tcp_header->dest) != PORT) {
            continue;
        }

        char src_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_header->saddr, src_ip, sizeof(src_ip));

        // printf("Client IP: %s\n", src_ip);
        // printf("Client Port: %u\n", ntohs(tcp_header->source));

        if (is_ip_allowed(src_ip)) {
            printf("Allowed %s:%u\n", src_ip, ntohs(tcp_header->source));
        } else {
            printf("Disallowed %s:%u\n", src_ip, ntohs(tcp_header->source));
            continue;
        }

        int tcp_header_len = tcp_header->doff * 4;
        int payload_len = bytes - (ip_header_len + tcp_header_len);
        if (payload_len > 0) {
            char *payload = buffer + ip_header_len + tcp_header_len;
            printf("Message: %.*s\n", payload_len, payload);
        }

        printf("\n");
    }

    close(server_fd);
    return 0;
}
