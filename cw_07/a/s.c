#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_IPS 100

int server_fd;
char *allowed_ips[MAX_IPS + 1];
int allowed_ip_count = 0;

float calculate(const float num1, const char operator, const float num2) {
    switch (operator) {
        case '+': return num1 + num2;
        case '-': return num1 - num2;
        case '*': return num1 * num2;
        case '/': return (num2 != 0) ? num1 / num2 : 0;
        default: return 0;
    }
}

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

void handle_sigint(int sig) {
    printf("\nClosing server\n");
    close(server_fd);
    exit(0);
}

int main() {
    struct sockaddr_in address, cliaddr;
    socklen_t clilen;
    char buffer[BUFFER_SIZE] = {0};

    signal(SIGINT, handle_sigint);

    if (load_ips_from_file("ip.txt") < 0) {
        fprintf(stderr, "Failed to load IP addresses from file.\n");
        exit(EXIT_FAILURE);
    }

    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Enable broadcast option
    int broadcast = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("setsockopt SO_BROADCAST failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Bind to all available interfaces
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("UDP Server running on port %d...\n", PORT);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        clilen = sizeof(cliaddr);
        int n = recvfrom(server_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &clilen);
        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }
        const char *client_ip = inet_ntoa(cliaddr.sin_addr);
        printf("Client IP: %s\n", client_ip);

        if (!is_ip_allowed(client_ip)) {
            printf("Received message from disallowed IP: %s\n", client_ip);
            continue;
        }

        float num1, num2;
        char operator;
        if (sscanf(buffer, "%f %c %f", &num1, &operator, &num2) == 3) {
            float result = calculate(num1, operator, num2);
            snprintf(buffer, BUFFER_SIZE, "%f", result);
            sendto(server_fd, buffer, strlen(buffer), 0, (struct sockaddr *)&cliaddr, clilen);
            printf("%f %c %f = %f\n", num1, operator, num2, result);
        } else {
            char *err_msg = "Invalid input.";
            sendto(server_fd, err_msg, strlen(err_msg), 0, (struct sockaddr *)&cliaddr, clilen);
        }
    }

    close(server_fd);
    return 0;
}
