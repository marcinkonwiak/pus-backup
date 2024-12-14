#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 2341
#define BUFFER_SIZE 1024
#define MAX_IPS 100

int sockfd;
char *allowed_ips[MAX_IPS + 1]; // null-terminated list
int allowed_ip_count = 0;

void handle_exit(int sig) {
    printf("\nClosing server\n");
    if (sockfd >= 0) {
        close(sockfd);
    }
    exit(0);
}

int load_ips_from_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp) && allowed_ip_count < MAX_IPS) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = '\0';

        // Skip empty lines
        if (line[0] == '\0') {
            continue;
        }

        // Duplicate the line and store it
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

// Function to check if a given IP address is in the allowed list
int is_ip_allowed(const char *ip) {
    for (int i = 0; allowed_ips[i] != NULL; i++) {
        if (strcmp(ip, allowed_ips[i]) == 0) {
            return 1; // allowed
        }
    }
    return 0; // not allowed
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;
    char buffer[BUFFER_SIZE];

    signal(SIGINT, handle_exit);

    // Load allowed IPs from file
    // Change "allowed_ips.txt" to the path of your file
    if (load_ips_from_file("ip.txt") < 0) {
        fprintf(stderr, "Failed to load IP addresses from file.\n");
        exit(EXIT_FAILURE);
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP Server listening for broadcasts on port %d...\n", PORT);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        addr_len = sizeof(client_addr);

        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &addr_len);
        if (n < 0) {
            perror("recvfrom error");
            continue;
        }

        buffer[n] = '\0';

        // Convert the client's IP to a string
        const char *client_ip = inet_ntoa(client_addr.sin_addr);

        // Check if the IP is allowed
        if (is_ip_allowed(client_ip)) {
            printf("Received allowed broadcast message: '%s' from %s:%d\n",
                buffer,
                client_ip,
                ntohs(client_addr.sin_port)
            );

            const char *response = "2";
            if (sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&client_addr, addr_len) < 0) {
                perror("sendto error");
            }
        } else {
            // If the IP is not allowed, just log it and do not respond
            printf("Received unauthorized message from %s:%d - ignoring.\n",
                client_ip,
                ntohs(client_addr.sin_port)
            );
        }
    }

    close(sockfd);
    return 0;
}
