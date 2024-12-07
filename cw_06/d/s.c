#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <syslog.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

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

void handle_sigint(int sig) {
    syslog(LOG_INFO, "UDP inetd server shutting down.");
    closelog();
    _exit(0);
}

int main() {
    openlog("udp_inetd_server", LOG_PID | LOG_CONS, LOG_USER);

    signal(SIGINT, handle_sigint);


    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    char buffer[BUFFER_SIZE] = {0};

    int n = recvfrom(0, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &clilen);
    if (n < 0) {
        syslog(LOG_ERR, "recvfrom failed.");
        closelog();
        return EXIT_FAILURE;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cliaddr.sin_addr, client_ip, INET_ADDRSTRLEN);
    syslog(LOG_INFO, "Received request from %s:%d", client_ip, ntohs(cliaddr.sin_port));

    float num1, num2;
    char operator;
    if (sscanf(buffer, "%f %c %f", &num1, &operator, &num2) == 3) {
        float result = calculate(num1, operator, num2);
        snprintf(buffer, BUFFER_SIZE, "%f", result);
        sendto(0, buffer, strlen(buffer), 0, (struct sockaddr *)&cliaddr, clilen);
        syslog(LOG_INFO, "%f %c %f = %f", num1, operator, num2, result);
    } else {
        const char *err_msg = "Invalid input.";
        sendto(0, err_msg, strlen(err_msg), 0, (struct sockaddr *)&cliaddr, clilen);
    }

    closelog();
    return 0;
}
