#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 8085
#define BUFFER_SIZE 1024

int server_fd;

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
float calculate(const float num1, const char operator, const float num2) {
    switch (operator) {
        case '+': return num1 + num2;
        case '-': return num1 - num2;
        case '*': return num1 * num2;
        case '/': return (num2 != 0) ? num1 / num2 : 0;
        default: return 0;
    }
}

void handle_client(int conn_fd) {
    char buffer[BUFFER_SIZE] = {0};
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int valread = read(conn_fd, buffer, BUFFER_SIZE);
        if (valread <= 0) {
            break;
        }

        float num1, num2;
        char operator;
        sscanf(buffer, "%f %c %f", &num1, &operator, &num2);
        float result = calculate(num1, operator, num2);

        snprintf(buffer, BUFFER_SIZE, "%f", result);
        send(conn_fd, buffer, strlen(buffer), 0);
    }
}

void handle_sigint(int sig) {
    printf("\nClosing server\n");
    close(server_fd);
    exit(0);
}

void sig_chld(int signo) {
    int stat;
}

int main() {
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (load_ips_from_file("ip.txt") < 0) {
        fprintf(stderr, "Failed to load IP addresses from file.\n");
        exit(EXIT_FAILURE);
    }
    char buffer[BUFFER_SIZE] = {0};
    int conn_fd;

    signal(SIGINT, handle_sigint);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, sig_chld);

    printf("Server running on port %d...\n", PORT);

    while (1) {
        if ((conn_fd = accept(server_fd, (struct sockaddr *) &address,
                              (socklen_t *) &addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }


        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);

        printf("Client IP: %s \n", client_ip);

        if (!is_ip_allowed(client_ip)) {
            printf("Received message from disallowed IP: %s\n", client_ip);
            continue;
        } else {
            printf("Valid IP\n");
        }

        if (fork() == 0) {
            close(server_fd);
            handle_client(conn_fd);
            close(conn_fd);
            exit(0);
        }

        close(conn_fd);
    }
}
