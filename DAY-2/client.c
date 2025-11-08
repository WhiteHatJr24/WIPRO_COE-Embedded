// client_day2.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000
#define BUF_SZ 4096

int main(int argc, char **argv) {
    if (argc<2) { fprintf(stderr,"Usage: %s <server-ip>\n", argv[0]); return 1; }
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sa = {0}; sa.sin_family=AF_INET; sa.sin_port = htons(PORT); inet_pton(AF_INET, argv[1], &sa.sin_addr);
    connect(s, (struct sockaddr*)&sa, sizeof(sa));
    send(s, "LIST\n", 5, 0);
    char buf[BUF_SZ];
    ssize_t n = recv(s, buf, sizeof(buf)-1, 0);
    if (n<=0) { printf("No response\n"); close(s); return 0; }
    buf[n]='\0';
    printf("Files on server:\n%s\n", buf);
    // Selection demonstration (client chooses a name to GET later)
    printf("Type filename you'd like to GET on next step (or press Enter to quit): ");
    char fname[256];
    if (!fgets(fname, sizeof(fname), stdin)) { close(s); return 0; }
    if (fname[0]=='\n') { close(s); return 0; }
    fname[strcspn(fname, "\n")] = 0;
    printf("You selected: %s\n", fname);
    close(s);
    return 0;
}
