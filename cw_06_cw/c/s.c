#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include<sys/types.h>
#include<sys/stat.h>

#define BUFFER_SIZE 1024

float calculate(const float num1, const char operator, const float num2) {
    switch (operator) {
        case '+': return num1 + num2;
        case '-': return num1 - num2;
        case '*': return num1 * num2;
        case '/': return (num2 != 0) ? num1 / num2 : 0;
        default: return 0;
    }
}

void handle_client() {
    char buffer[BUFFER_SIZE] = {0};
    while (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
        float num1, num2;
        char operator;
        if (sscanf(buffer, "%f %c %f", &num1, &operator, &num2) != 3) {
            fprintf(stdout, "Invalid input\n");
            fflush(stdout);
            continue;
        }

        float result = calculate(num1, operator, num2);
        syslog(LOG_INFO, "Result: %f", result);
        fprintf(stdout, "%f\n", result);
        fflush(stdout);
    }
}

void daemon_init(const char *pname, int facility)
{
    pid_t	pid;
    if ((pid = fork()) != 0)
        exit(0);
    setsid();
    signal(SIGHUP, SIG_IGN);
    if ((pid = fork()) != 0)
        exit(0);

    chdir("/");
    umask(0);

    for (int i = 0; i < 64; i++)
        close(i);
    openlog(pname, LOG_PID, facility);
}

int main() {
    openlog("inetd_server", LOG_PID | LOG_CONS, LOG_USER);

    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    if (getpeername(STDIN_FILENO, (struct sockaddr *)&addr, &addrlen) == 0) {
        char client_ip[INET6_ADDRSTRLEN];
        int client_port = 0;

        if (addr.ss_family == AF_INET) {
            struct sockaddr_in *s = (struct sockaddr_in *)&addr;
            inet_ntop(AF_INET, &s->sin_addr, client_ip, sizeof(client_ip));
            client_port = ntohs(s->sin_port);
        } else if (addr.ss_family == AF_INET6) {
            struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
            inet_ntop(AF_INET6, &s->sin6_addr, client_ip, sizeof(client_ip));
            client_port = ntohs(s->sin6_port);
        } else {
            snprintf(client_ip, sizeof(client_ip), "Unknown");
        }

        syslog(LOG_INFO, "Client connected from %s:%d", client_ip, client_port);
    } else {
        syslog(LOG_INFO, "Client connected, but unable to determine peer address.");
    }

    handle_client();
    syslog(LOG_INFO, "Client disconnected.");
    closelog();

    return 0;
}
