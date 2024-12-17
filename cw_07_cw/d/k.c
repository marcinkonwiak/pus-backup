#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <sys/socket.h>

unsigned short csum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
    for (; nwords > 0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum &0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

int main() {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int one = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    const char *source_ip = "192.168.1.11";    // Client IP
    const char *dest_ip   = "127.0.0.1";  // Server IP
    
    if (inet_pton(AF_INET, dest_ip, &sockaddr.sin_addr) <= 0) {
        printf("\nInvalid address\n");
        return -1;
    }

    unsigned short source_port = 12345;   // Client port
    unsigned short dest_port = 8085;      // Server port

    char packet[4096];
    memset(packet, 0, sizeof(packet));

    struct iphdr *ip = (struct iphdr *)packet;
    struct tcphdr *tcp = (struct tcphdr *)(packet + sizeof(struct iphdr));
    char *data = (char *)(packet + sizeof(struct iphdr) + sizeof(struct tcphdr));
    strcpy(data, "1 + 1");

    ip->ihl = 5;
    ip->version = 4;
    ip->tos = 0;
    ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr) + strlen(data));
    ip->id = htons(54321);
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = IPPROTO_TCP;
    ip->saddr = inet_addr(source_ip);
    ip->daddr = inet_addr(dest_ip);
    ip->check = 0;

    tcp->source = htons(source_port);
    tcp->dest = htons(dest_port);
    tcp->seq = htonl(0);
    tcp->ack_seq = 0;
    tcp->doff = 5;
    tcp->syn = 1;
    tcp->window = htons(65535);
    tcp->check = 0;
    tcp->urg_ptr = 0;


    struct pseudo_header {
        unsigned int src_addr;
        unsigned int dst_addr;
        unsigned char placeholder;
        unsigned char protocol;
        unsigned short tcp_length;
    } psh;

    psh.src_addr = inet_addr(source_ip);
    psh.dst_addr = inet_addr(dest_ip);
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(struct tcphdr) + strlen(data));

    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr) + strlen(data);
    char *pseudogram = malloc(psize);
    memcpy(pseudogram, (char *)&psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcp, sizeof(struct tcphdr) + strlen(data));

    tcp->check = csum((unsigned short *)pseudogram, psize/2);
    ip->check = csum((unsigned short *)packet, (sizeof(struct iphdr))/2);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = tcp->dest;
    sin.sin_addr.s_addr = ip->daddr;

    if (sendto(sockfd, packet, ntohs(ip->tot_len), 0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    printf("sent\n");

    free(pseudogram);
    close(sockfd);
    return 0;
}
