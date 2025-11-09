// Day-4/client.c
// Connects, shows server list, then allows GET or PUT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8080
#define BUFSZ 8192

ssize_t recv_line(int fd, char *out, size_t maxlen) {
    size_t idx = 0;
    while (idx + 1 < maxlen) {
        ssize_t r = read(fd, out + idx, 1);
        if (r <= 0) return (r == 0 && idx>0) ? idx : -1;
        if (out[idx] == '\n') { idx++; out[idx] = '\0'; return idx; }
        idx++;
    }
    out[maxlen-1] = '\0';
    return idx;
}

ssize_t send_all(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    const char *b = buf;
    while (sent < len) {
        ssize_t s = write(fd, b + sent, len - sent);
        if (s <= 0) return -1;
        sent += s;
    }
    return sent;
}

int main(void) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }
    struct sockaddr_in serv = {0};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORT);
    serv.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(s, (struct sockaddr*)&serv, sizeof(serv)) < 0) { perror("connect"); close(s); return 1; }
    char line[1024];

    // check dir status
    if (recv_line(s, line, sizeof(line)) <= 0) { fprintf(stderr, "no response\n"); close(s); return 1; }
    if (strcmp(line, "OKAY\n") != 0) { printf("Server error: %s\n", line); close(s); return 1; }

    printf("Files on server:\n");
    while (1) {
        if (recv_line(s, line, sizeof(line)) <= 0) break;
        if (strcmp(line, "END\n") == 0) break;
        printf(" - %s", line);
    }

    printf("\nChoose action: (1) GET  (2) PUT  : ");
    int choice = 0;
    if (scanf("%d%*c", &choice) != 1) { close(s); return 1; }

    if (choice == 1) {
        printf("Enter filename to download: ");
        char fname[512];
        if (!fgets(fname, sizeof(fname), stdin)) { close(s); return 1; }
        fname[strcspn(fname, "\n")] = 0;
        char cmd[600]; snprintf(cmd, sizeof(cmd), "GET %s\n", fname);
        send_all(s, cmd, strlen(cmd));
        // read response
        if (recv_line(s, line, sizeof(line)) <= 0) { fprintf(stderr,"noresp\n"); close(s); return 1; }
        if (strcmp(line, "NOFILE\n") == 0) { printf("Server: NOFILE\n"); close(s); return 0; }
        if (strcmp(line, "FILEOK\n") != 0) { printf("Unexpected: %s\n", line); close(s); return 1; }
        // size
        if (recv_line(s, line, sizeof(line)) <= 0) { fprintf(stderr,"no size\n"); close(s); return 1; }
        long size = atol(line);
        printf("Receiving %ld bytes...\n", size);
        int fd = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) { perror("open"); close(s); return 1; }
        long got = 0;
        char buf[BUFSZ];
        while (got < size) {
            ssize_t toread = (size - got) > BUFSZ ? BUFSZ : (size - got);
            ssize_t r = read(s, buf, toread);
            if (r <= 0) break;
            write(fd, buf, r);
            got += r;
        }
        close(fd);
        printf("Saved '%s' (%ld bytes)\n", fname, got);
    } else if (choice == 2) {
        printf("Enter local filename to upload: ");
        char fname[512];
        if (!fgets(fname, sizeof(fname), stdin)) { close(s); return 1; }
        fname[strcspn(fname, "\n")] = 0;
        struct stat st;
        if (stat(fname, &st) < 0) { perror("stat"); close(s); return 1; }
        char cmd[600]; snprintf(cmd, sizeof(cmd), "PUT %s\n", fname);
        send_all(s, cmd, strlen(cmd));
        // wait READY
        if (recv_line(s, line, sizeof(line)) <= 0) { fprintf(stderr,"noresp\n"); close(s); return 1; }
        if (strcmp(line, "READY\n") != 0) { printf("Server not ready: %s\n", line); close(s); return 1; }
        // send size\n then raw bytes
        char meta[64]; snprintf(meta, sizeof(meta), "%ld\n", (long)st.st_size);
        send_all(s, meta, strlen(meta));
        int fd = open(fname, O_RDONLY);
        if (fd < 0) { perror("open"); close(s); return 1; }
        ssize_t r;
        char buf[BUFSZ];
        while ((r = read(fd, buf, sizeof(buf))) > 0) {
            if (send_all(s, buf, r) < 0) { perror("send"); break; }
        }
        close(fd);
        // read result
        if (recv_line(s, line, sizeof(line)) > 0) {
            printf("Server: %s", line);
        }
    } else {
        printf("Invalid choice\n");
    }

    close(s);
    return 0;
}
