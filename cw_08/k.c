#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <errno.h>

#define SOCKET_PATH "/home/admin/Code/studia/pus/pus-backup/cw_08/server_socket"
#define BUFFER_SIZE 256

static int client_fd = -1;

// Simple signal handler to allow Ctrl+C to cleanly exit
void handle_sigint(int sig) {
    (void)sig;  // Unused
    printf("\nClient shutting down...\n");
    if (client_fd != -1) {
        close(client_fd);
    }
    exit(EXIT_SUCCESS);
}

int main() {
    struct sockaddr_un server_addr;
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    if ((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server. Enter math operations (Ctrl + C to quit):\n\n");

    while (1) {
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));

        printf("Enter operation (e.g., 3 + 4): ");
        fflush(stdout);

        if (!fgets(buffer, sizeof(buffer), stdin)) {
            printf("\nClient shutting down...\n");
            break;
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        if (write(client_fd, buffer, strlen(buffer)) == -1) {
            perror("write");
            break;
        }

        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            if (bytes_read < 0) {
                perror("read");
            } else {
                printf("Server closed the connection.\n");
            }
            break;
        }

        printf("%s", buffer);
    }

    close(client_fd);
    return 0;
}
