#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define BUFSIZE 1500

static int sockfd;
static pid_t pid;
static struct sockaddr_in dest_addr;

static unsigned short in_cksum(unsigned short *addr, int len);

void handle_exit(int sig) {
    printf("\nClosing\n");
    close(sockfd);
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

int main() {
    char sendbuf[BUFSIZE], recvbuf[BUFSIZE];
    struct ip *ip_hdr;
    struct icmp *icmp_hdr;
    setuid(getuid());

    signal(SIGINT, handle_exit);

    // Create raw socket for ICMP
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        perror("socket");
        return -1;
    }

    // Set destination
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;

    // Change this to your server's IP
    if (inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr) <= 0) {
        printf("\nInvalid address\n");
        return -1;
    }

    pid = getpid();

    printf("Raw ICMP Calculator Client\n");
    printf("Note: Running as root may be required.\n");

    while (1) {
        printf("First number: ");
        float num1 = get_float_input();
        printf("Enter operator (+, -, *, /): ");
        char operator = get_operator_input();
        printf("Second number: ");
        float num2 = get_float_input();

        // Construct ICMP request packet with fake IP source
        memset(sendbuf, 0, sizeof(sendbuf));
        ip_hdr = (struct ip *)sendbuf;
        icmp_hdr = (struct icmp *)(sendbuf + sizeof(struct ip));

        // Fake source IP can be set by enabling IP_HDRINCL and providing a crafted IP header
        int on = 1;
        if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
            perror("setsockopt IP_HDRINCL");
            return -1;
        }

        // Fill IP header
        ip_hdr->ip_hl = 5;
        ip_hdr->ip_v = 4;
        ip_hdr->ip_tos = 0;
        // length will be set after building ICMP
        ip_hdr->ip_len = 0;
        ip_hdr->ip_id = htons(rand() % 65535);
        ip_hdr->ip_off = 0;
        ip_hdr->ip_ttl = 64;
        ip_hdr->ip_p = IPPROTO_ICMP;
        // Fake source IP, for example: 1.2.3.4
        ip_hdr->ip_src.s_addr = inet_addr("127.0.0.1");
        ip_hdr->ip_dst = dest_addr.sin_addr;
        ip_hdr->ip_sum = 0; // computed later

        // Fill ICMP header
        icmp_hdr->icmp_type = ICMP_ECHO;
        icmp_hdr->icmp_code = 0;
        icmp_hdr->icmp_id = pid;
        icmp_hdr->icmp_seq = rand() % 65535; // arbitrary sequence

        // Put calculation request in the ICMP data
        char *data = (char *)icmp_hdr->icmp_data;
        snprintf(data, BUFSIZE - sizeof(struct ip) - 8, "%f %c %f", num1, operator, num2);
        int icmp_len = 8 + strlen(data);

        icmp_hdr->icmp_cksum = 0;
        icmp_hdr->icmp_cksum = in_cksum((unsigned short *)icmp_hdr, icmp_len);

        int packet_len = sizeof(struct ip) + icmp_len;
        ip_hdr->ip_len = htons(packet_len);

        // Checksum
        unsigned short *ip_hdr_ptr = (unsigned short *)ip_hdr;
        int iphdr_len = sizeof(struct ip);
        int sum = 0;
        for (int i = 0; i < iphdr_len/2; i++)
            sum += ip_hdr_ptr[i];
        sum = (sum >> 16) + (sum & 0xffff);
        sum += (sum >> 16);
        ip_hdr->ip_sum = ~sum;

        // Send the packet
        if (sendto(sockfd, sendbuf, packet_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            perror("sendto");
            continue;
        }

        for (;;) {
            // Wait for reply
            struct sockaddr_in rsrc;
            socklen_t rlen = sizeof(rsrc);
            ssize_t n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&rsrc, &rlen);
            if (n <= 0) {
                perror("recvfrom");
                continue;
            }

            struct ip *rip = (struct ip *)recvbuf;
            int riphdrlen = rip->ip_hl << 2;
            struct icmp *ricmp = (struct icmp *)(recvbuf + riphdrlen);
            char *result_str = (char *)ricmp->icmp_data;
            printf("Result: %s\n", result_str);
            // isEchoReply
            printf("Request: %f %c %f = %f\n", num1, operator, num2, atof(result_str));


            // if (ricmp->icmp_type == ICMP_ECHOREPLY && ricmp->icmp_id == pid) {
            //     char *result_str = (char *)ricmp->icmp_data;
            //     printf("Result: %s\n", result_str);
            //     break;  // We got our reply
            // }
            // Otherwise, keep looping until we find the reply
        }
    }

    return 0;
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
