#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// Pseudo header for TCP checksum
struct pseudo_header {
    u_int32_t source_address;
    u_int32_t dest_address;
    u_int8_t placeholder;
    u_int8_t protocol;
    u_int16_t tcp_length;
};

float calculate(const float num1, const char operator, const float num2) {
    switch (operator) {
        case '+': return num1 + num2;
        case '-': return num1 - num2;
        case '*': return num1 * num2;
        case '/': return (num2 != 0) ? num1 / num2 : 0;
        default: return 0;
    }
}

unsigned short csum(unsigned short *ptr,int nbytes) {
    register long sum;
    unsigned short oddbyte;
    register unsigned short answer;

    sum=0;
    while(nbytes>1) {
        sum+=*ptr++;
        nbytes-=2;
    }
    if(nbytes==1) {
        oddbyte=0;
        *((u_char*)&oddbyte)=*(u_char*)ptr;
        sum+=oddbyte;
    }

    sum = (sum>>16)+(sum &0xffff);
    sum = sum+(sum>>16);
    answer=(unsigned short)~sum;

    return(answer);
}

int main() {
    int sockfd;
    unsigned char buffer[65535];
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    // Create a RAW socket that listens for all IP packets (then we filter by protocol)
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // This address represents our server's IP and port
    // Adjust according to your environment if needed.
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("Server running on port %d...\n", PORT);

    while (1) {
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addrlen);
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

        // Check if this packet is intended for our server port
        if (ntohs(tcp->dest) != PORT) {
            continue;
        }

        int payload_len = n - (ip_header_len + tcp_header_len);
        unsigned char *payload = (unsigned char *)(buffer + ip_header_len + tcp_header_len);

        if (payload_len > 0) {
            float num1, num2;
            char operator;
            if (sscanf((char *)payload, "%f %c %f", &num1, &operator, &num2) != 3) {
                // Not a valid input
                continue;
            }

            float result = calculate(num1, operator, num2);
            printf("%f %c %f = %f\n", num1, operator, num2, result);

            // Prepare response
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "%f", result);
            int response_len = (int)strlen(response);

            // To send a crafted packet, we need a new socket with IP_HDRINCL
            int sendfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
            if (sendfd < 0) {
                perror("socket for sending");
                continue;
            }

            int one = 1;
            if (setsockopt(sendfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
                perror("setsockopt IP_HDRINCL");
                close(sendfd);
                continue;
            }

            unsigned char response_packet[4096];
            memset(response_packet, 0, sizeof(response_packet));

            struct iphdr *iph = (struct iphdr *)response_packet;
            struct tcphdr *tcph = (struct tcphdr *)(response_packet + sizeof(struct iphdr));
            char *data = (char *)(response_packet + sizeof(struct iphdr) + sizeof(struct tcphdr));
            strcpy(data, response);

            // Build IP header
            iph->ihl = 5;
            iph->version = 4;
            iph->tos = 0;
            iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr) + response_len);
            iph->id = htons(54321);
            iph->frag_off = 0;
            iph->ttl = 64;
            iph->protocol = IPPROTO_TCP;
            iph->saddr = server_addr.sin_addr.s_addr; // server IP
            iph->daddr = client_addr.sin_addr.s_addr; // client IP
            iph->check = 0;
            iph->check = csum((unsigned short *)iph, sizeof(struct iphdr));

            // Build TCP header
            tcph->source = server_addr.sin_port;
            tcph->dest = tcp->source;
            tcph->seq = htonl(1);
            tcph->ack_seq = 0;
            tcph->doff = 5; // no options
            // We'll just set PSH/ACK to 0 for simplicity since no handshake was done
            tcph->fin = 0;
            tcph->syn = 0;
            tcph->rst = 0;
            tcph->psh = 1;
            tcph->ack = 0;
            tcph->urg = 0;
            tcph->window = htons(5840);
            tcph->check = 0;
            tcph->urg_ptr = 0;

            // Compute TCP checksum with pseudo-header
            struct pseudo_header psh;
            psh.source_address = iph->saddr;
            psh.dest_address = iph->daddr;
            psh.placeholder = 0;
            psh.protocol = IPPROTO_TCP;
            psh.tcp_length = htons(sizeof(struct tcphdr) + response_len);

            int pseudo_size = sizeof(struct pseudo_header) + sizeof(struct tcphdr) + response_len;
            unsigned char *pseudo_buf = malloc(pseudo_size);
            if (!pseudo_buf) {
                perror("malloc");
                close(sendfd);
                continue;
            }

            memcpy(pseudo_buf, &psh, sizeof(struct pseudo_header));
            memcpy(pseudo_buf + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr) + response_len);
            tcph->check = csum((unsigned short*)pseudo_buf, pseudo_size);
            free(pseudo_buf);

            // Send packet
            if (sendto(sendfd, response_packet, ntohs(iph->tot_len), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                perror("sendto failed");
            } else {
                printf("Response sent\n");
            }

            close(sendfd);

        } else {
            printf("No payload.\n");
        }
    }

    close(sockfd);
    return 0;
}
