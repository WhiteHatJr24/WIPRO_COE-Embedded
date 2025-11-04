#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server-ip> <port> <client-name>\n", argv[0]);
        return 1;
    }
    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    const char *client_name = argv[3];

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &srv.sin_addr) <= 0) { perror("inet_pton"); return 1; }

    if (connect(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) { perror("connect"); return 1; }

    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "HELLO %s\n", client_name);
    send(sock, buf, strlen(buf), 0);

    ssize_t r = recv(sock, buf, sizeof(buf)-1, 0);
    if (r > 0) {
        buf[r] = '\0';
        printf("Server replied: %s", buf);
    } else {
        printf("No reply or error\n");
    }

    close(sock);
    return 0;
}
