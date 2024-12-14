#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <errno.h>

#define BUFSIZE 1500

static int sockfd;
static pid_t pid;

static unsigned short in_cksum(unsigned short *addr, int len);
static void handle_sigint(int sig);

static float calculate(const float num1, const char operator, const float num2) {
    switch (operator) {
        case '+': return num1 + num2;
        case '-': return num1 - num2;
        case '*': return num1 * num2;
        case '/': return (num2 != 0) ? num1 / num2 : 0;
        default: return 0;
    }
}

int main() {
    int size = 60*1024;
    struct icmp *icmp;
    struct ip *ip;
    char recvbuf[BUFSIZE], sendbuf[BUFSIZE];
    ssize_t n;
    socklen_t len;
    struct sockaddr_in sa_src;
    struct sockaddr_in sa_dest;
    pid = getpid();

    signal(SIGINT, handle_sigint);

    // Create raw ICMP socket
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    // Increase the receive buffer size
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0) {
        perror("setsockopt error");
        exit(EXIT_FAILURE);
    }

    printf("Raw ICMP calculator server running...\nPress Ctrl+C to stop.\n");

    for (;;) {
        len = sizeof(sa_src);
        n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&sa_src, &len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("recvfrom error");
            exit(EXIT_FAILURE);
        }

        ip = (struct ip *)recvbuf;
        int ip_header_len = ip->ip_hl << 2;
        icmp = (struct icmp *)(recvbuf + ip_header_len);
        int icmplen = n - ip_header_len;

        if (icmplen < 8) {
            // Not a valid ICMP packet
            continue;
        }

        if (icmp->icmp_type == ICMP_ECHO && icmplen > 8) {
            // Extract calculation request from ICMP payload
            // Payload layout: "num1 operator num2"
            char *payload = (char *)icmp->icmp_data;
            float num1, num2;
            char operator;
            if (sscanf(payload, "%f %c %f", &num1, &operator, &num2) == 3) {
                float result = calculate(num1, operator, num2);
                printf("Request: %f %c %f = %f\n", num1, operator, num2, result);

                // Prepare ICMP echo reply
                memset(sendbuf, 0, sizeof(sendbuf));
                struct ip *ip_reply = (struct ip *)sendbuf;
                struct icmp *icmp_reply = (struct icmp *)(sendbuf + sizeof(struct ip));

                // Construct IP header
                ip_reply->ip_hl = 5;
                ip_reply->ip_v = 4;
                ip_reply->ip_tos = 0;
                ip_reply->ip_len = htons(sizeof(struct ip) + 8 + strlen(payload));
                ip_reply->ip_id = 0;
                ip_reply->ip_off = 0;
                ip_reply->ip_ttl = 64;
                ip_reply->ip_p = IPPROTO_ICMP;
                ip_reply->ip_sum = 0;
                ip_reply->ip_src = ip->ip_dst;
                ip_reply->ip_dst = ip->ip_src;

                // Construct ICMP reply
                icmp_reply->icmp_type = ICMP_ECHOREPLY;
                icmp_reply->icmp_code = 0;
                icmp_reply->icmp_id = icmp->icmp_id;
                icmp_reply->icmp_seq = icmp->icmp_seq;

                // Put the result in the payload of the ICMP echo reply
                snprintf((char *)icmp_reply->icmp_data, BUFSIZE - sizeof(struct ip) - 8, "%f", result);
                snprintf((char *)icmp_reply->icmp_data, BUFSIZE - sizeof(struct ip) - 8, "%f", result);

                int icmp_len = 8 + strlen((char *)icmp_reply->icmp_data);
                icmp_reply->icmp_cksum = 0;
                icmp_reply->icmp_cksum = in_cksum((unsigned short *)icmp_reply, icmp_len);

                // Set correct IP length now that we know the payload size
                ip_reply->ip_len = htons(sizeof(struct ip) + icmp_len);

                // Compute IP checksum
                unsigned short *ip_hdr = (unsigned short *)ip_reply;
                int iphdr_len = sizeof(struct ip);
                int sum = 0;
                for (int i = 0; i < iphdr_len/2; i++)
                    sum += ip_hdr[i];
                sum = (sum >> 16) + (sum & 0xffff);
                sum += (sum >> 16);
                ip_reply->ip_sum = ~sum;

                // Destination address structure
                memset(&sa_dest, 0, sizeof(sa_dest));
                sa_dest.sin_family = AF_INET;
                sa_dest.sin_addr = ip_reply->ip_dst;

                // Send reply
                if (sendto(sockfd, sendbuf, sizeof(struct ip) + icmp_len, 0,
                           (struct sockaddr *)&sa_dest, sizeof(sa_dest)) < 0) {
                    perror("sendto error");
                }
            }
        }
    }

    return 0;
}

void handle_sigint(int sig) {
    printf("\nClosing server\n");
    close(sockfd);
    exit(0);
}

static unsigned short in_cksum(unsigned short *addr, int len) {
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }
    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w ;
        sum += answer;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return(answer);
}
