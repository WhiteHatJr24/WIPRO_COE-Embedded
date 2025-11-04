#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define BACKLOG 5
#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(listenfd, BACKLOG) < 0) { perror("listen"); return 1; }

    printf("Server listening on port %d...\n", port);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t cli_len = sizeof(cli);
        int conn = accept(listenfd, (struct sockaddr*)&cli, &cli_len);
        if (conn < 0) { perror("accept"); continue; }

        char cli_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, cli_ip, sizeof(cli_ip));
        printf("Accepted connection from %s:%d\n", cli_ip, ntohs(cli.sin_port));

        char buf[BUF_SIZE];
        ssize_t r = recv(conn, buf, BUF_SIZE-1, 0);
        if (r <= 0) { close(conn); continue; }
        buf[r] = '\0';
        printf("Received: %s", buf);

        // Simple handshake: expect HELLO <name>\n
        if (strncmp(buf, "HELLO", 5) == 0) {
            const char *reply = "WELCOME NFS-SERVER\n";
            send(conn, reply, strlen(reply), 0);
        } else {
            const char *reply = "UNKNOWN\n";
            send(conn, reply, strlen(reply), 0);
        }

        close(conn);
        printf("Connection closed.\n");
    }

    close(listenfd);
    return 0;
}
