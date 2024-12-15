#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int sock = 0;

void handle_exit(int sig) {
    printf("\nClosing\n");
    close(sock);
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
        if (fgets(input, sizeof(input), stdin) != NULL && 
            (strlen(input) == 2 || (strlen(input) == 1 && input[0] != '\n')) &&
            (input[0] == '+' || input[0] == '-' || input[0] == '*' || input[0] == '/')) {
            return input[0];
        }
        printf("Invalid operator\n");
    }
}

unsigned short csum(unsigned short *buf, int len) {
    unsigned long sum = 0;
    for (; len > 1; len -= 2) {
        sum += *buf++;
    }
    if (len == 1) {
        sum += *(unsigned char*)buf;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

// Pseudo header for TCP checksum calculation
struct pseudo_header {
    u_int32_t source_address;
    u_int32_t dest_address;
    u_int8_t placeholder;
    u_int8_t protocol;
    u_int16_t tcp_length;
};

int main() {
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE] = {0};
    char message[100];

    signal(SIGINT, handle_exit);

    // Create a raw socket that can also receive TCP packets
    // Use IPPROTO_TCP so that incoming TCP packets can be received.
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt failed");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    while (1) {
        printf("First number: ");
        float num1 = get_float_input();
        printf("Enter operator (+, -, *, /): ");
        char operator = get_operator_input();
        printf("Second number: ");
        float num2 = get_float_input();

        snprintf(message, sizeof(message), "%f %c %f", num1, operator, num2);
        int data_len = (int)strlen(message);

        char packet[4096];
        memset(packet, 0, 4096);

        struct iphdr *iph = (struct iphdr *)packet;
        struct tcphdr *tcph = (struct tcphdr *)(packet + sizeof(struct iphdr));
        char *data = packet + sizeof(struct iphdr) + sizeof(struct tcphdr);
        strcpy(data, message);

        // Fake source IP and port
        const char *fake_source_ip = "1.2.3.4";
        unsigned short fake_source_port = 12345;

        // Build IP header
        iph->ihl = 5;
        iph->version = 4;
        iph->tos = 0;
        iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr) + data_len);
        iph->id = htons(54321);
        iph->frag_off = 0;
        iph->ttl = 64;
        iph->protocol = IPPROTO_TCP;
        iph->saddr = inet_addr(fake_source_ip);
        iph->daddr = server_address.sin_addr.s_addr;
        iph->check = 0;
        iph->check = csum((unsigned short *)iph, sizeof(struct iphdr));

        // Build TCP header
        tcph->source = htons(fake_source_port);
        tcph->dest = htons(PORT);
        tcph->seq = htonl(1);
        tcph->ack_seq = 0;
        tcph->doff = 5; // no options
        tcph->fin = 0;
        // We'll set syn=1 for the first packet, just to simulate initiating something
        tcph->syn = 1;
        tcph->rst = 0;
        tcph->psh = 0;
        tcph->ack = 0;
        tcph->urg = 0;
        tcph->window = htons(5840);
        tcph->check = 0;
        tcph->urg_ptr = 0;

        // Compute TCP checksum
        struct pseudo_header psh;
        psh.source_address = iph->saddr;
        psh.dest_address = iph->daddr;
        psh.placeholder = 0;
        psh.protocol = IPPROTO_TCP;
        psh.tcp_length = htons(sizeof(struct tcphdr) + data_len);

        int pseudo_size = sizeof(struct pseudo_header) + sizeof(struct tcphdr) + data_len;
        unsigned char *pseudo_buf = malloc(pseudo_size);
        if (!pseudo_buf) {
            perror("malloc");
            continue;
        }
        memcpy(pseudo_buf, &psh, sizeof(struct pseudo_header));
        memcpy(pseudo_buf + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr) + data_len);

        tcph->check = csum((unsigned short*)pseudo_buf, pseudo_size);
        free(pseudo_buf);

        // Send the packet
        if (sendto(sock, packet, ntohs(iph->tot_len), 0, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
            perror("sendto failed");
        } else {
            printf("Packet Sent\n");
        }

        // Attempt to read a response
        // Without a proper TCP handshake, this may or may not yield anything.
        memset(buffer, 0, sizeof(buffer));
        ssize_t rlen = read(sock, buffer, BUFFER_SIZE);
        if (rlen > 0) {
            // We received some raw IP packet
            struct iphdr *r_iph = (struct iphdr *)buffer;
            int ip_header_len = r_iph->ihl * 4;
            if (r_iph->protocol == IPPROTO_TCP && rlen >= ip_header_len + (int)sizeof(struct tcphdr)) {
                struct tcphdr *r_tcph = (struct tcphdr *)(buffer + ip_header_len);
                int tcp_header_len = r_tcph->doff * 4;
                int payload_len = rlen - ip_header_len - tcp_header_len;
                if (payload_len > 0) {
                    char *r_data = (char *)(buffer + ip_header_len + tcp_header_len);
                    // Print received payload
                    printf("Result: %.*s\n", payload_len, r_data);
                } else {
                    printf("Received a TCP packet with no payload.\n");
                }
            } else {
                printf("Received a non-TCP packet or malformed packet.\n");
            }
        } else if (rlen < 0) {
            perror("read");
        } else {
            printf("No data received.\n");
        }
    }

    return 0;
}
