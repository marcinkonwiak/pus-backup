#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <errno.h>

#define SOCKET_PATH "/home/admin/Code/studia/pus/pus-backup/cw_08/server_socket"
#define BACKLOG 5
#define BUFFER_SIZE 256

static int server_fd = -1;

void clean_up() {
    if (server_fd != -1) {
        close(server_fd);
    }
    unlink(SOCKET_PATH);
}

void handle_sigint(int sig) {
    (void)sig;
    printf("\nServer shutting down...\n");
    clean_up();
    exit(EXIT_SUCCESS);
}

double compute_result(double num1, char op, double num2, int *error) {
    *error = 0;
    double result = 0.0;
    switch(op) {
        case '+': result = num1 + num2; break;
        case '-': result = num1 - num2; break;
        case '*': result = num1 * num2; break;
        case '/':
            if (num2 == 0) {
                *error = 1;
            } else {
                result = num1 / num2;
            }
            break;
        default:
            *error = 1;
            break;
    }
    return result;
}

int main() {
    struct sockaddr_un server_addr;
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    unlink(SOCKET_PATH);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        clean_up();
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        clean_up();
        exit(EXIT_FAILURE);
    }

    printf("Server listening on %s ...\n", SOCKET_PATH);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR) {
                clean_up();
                exit(EXIT_SUCCESS);
            }
            perror("accept");
            continue;
        }

        printf("Client connected.\n");

        while (1) {
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, sizeof(buffer));

            ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read <= 0) {
                if (bytes_read < 0) {
                    perror("read");
                }
                close(client_fd);
                break;
            }

            double num1, num2;
            char op;
            int parsed = sscanf(buffer, "%lf %c %lf", &num1, &op, &num2);

            if (parsed != 3) {
                const char *err_msg = "Error: Invalid input format.\n";
                write(client_fd, err_msg, strlen(err_msg));
                continue;
            }

            int error_flag = 0;
            double result = compute_result(num1, op, num2, &error_flag);

            printf("%.2f %c %.2f = %.2f\n", num1, op, num2, result);

            char response[BUFFER_SIZE];
            if (error_flag) {
                snprintf(response, sizeof(response), "Error: Invalid operation.\n");
            } else {
                snprintf(response, sizeof(response), "Result: %.2f\n", result);
            }

            write(client_fd, response, strlen(response));
        }

        printf("Client disconnected.\n");
    }

    clean_up();
    return 0;
}
