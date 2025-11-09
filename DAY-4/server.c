// Day-4/server.c
// LIST, GET, PUT - binary safe
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define SHARE_DIR "./share"
#define BUFSZ 8192

// read a line ending with '\n' (includes '\n' in out). returns bytes read or -1
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
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(srv); return 1; }
    if (listen(srv, 5) < 0) { perror("listen"); close(srv); return 1; }

    printf("Day-4 server: listening on port %d\n", PORT);

    struct sockaddr_in cli;
    socklen_t cli_len = sizeof(cli);
    int c = accept(srv, (struct sockaddr*)&cli, &cli_len);
    if (c < 0) { perror("accept"); close(srv); return 1; }
    printf("Client connected\n");

    // First: respond to LIST or GET/PUT request flow.
    // We'll first send LIST automatically (as previous days did), then handle client's command.

    // Send directory status
    DIR *d = opendir(SHARE_DIR);
    if (!d) {
        const char *err = "ERR_DIR\n";
        send_all(c, err, strlen(err));
        perror("opendir");
        close(c); close(srv); return 1;
    } else {
        send_all(c, "OKAY\n", 5);
    }

    // Send file list (one filename per line), end with "END\n"
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_type == DT_REG) {
            send_all(c, e->d_name, strlen(e->d_name));
            send_all(c, "\n", 1);
        }
    }
    closedir(d);
    send_all(c, "END\n", 4);

    // Read command from client: either "GET filename\n" or "PUT filename\n"
    char line[512];
    if (recv_line(c, line, sizeof(line)) <= 0) {
        fprintf(stderr, "failed to read command\n");
        close(c); close(srv); return 1;
    }
    // strip newline
    line[strcspn(line, "\n")] = 0;
    if (strncmp(line, "GET ", 4) == 0) {
        char *fname = line + 4;
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", SHARE_DIR, fname);
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            send_all(c, "NOFILE\n", 7);
            fprintf(stderr, "GET: open failed: %s\n", strerror(errno));
        } else {
            struct stat st;
            fstat(fd, &st);
            // send FILEOK\n<size>\n then raw bytes
            send_all(c, "FILEOK\n", 7);
            char meta[64]; snprintf(meta, sizeof(meta), "%ld\n", (long)st.st_size);
            send_all(c, meta, strlen(meta));
            // stream file
            ssize_t r;
            char buf[BUFSZ];
            while ((r = read(fd, buf, sizeof(buf))) > 0) {
                if (send_all(c, buf, r) < 0) { perror("send"); break; }
            }
            close(fd);
            printf("Sent file '%s' (%ld bytes)\n", fname, (long)st.st_size);
        }
    } else if (strncmp(line, "PUT ", 4) == 0) {
        char *fname = line + 4;
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", SHARE_DIR, fname);
        // respond ready
        send_all(c, "READY\n", 6);
        // next line should be size\n
        if (recv_line(c, line, sizeof(line)) <= 0) {
            fprintf(stderr, "PUT: failed to read size\n");
        } else {
            long size = atol(line);
            int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd < 0) {
                send_all(c, "ERR\n", 4);
                fprintf(stderr, "PUT: open target failed\n");
            } else {
                long got = 0;
                char buf[BUFSZ];
                while (got < size) {
                    ssize_t toread = (size - got) > BUFSZ ? BUFSZ : (size - got);
                    ssize_t r = read(c, buf, toread);
                    if (r <= 0) break;
                    write(fd, buf, r);
                    got += r;
                }
                close(fd);
                if (got == size) send_all(c, "OK\n", 3);
                else send_all(c, "ERR\n", 4);
                printf("Received file '%s' (%ld bytes)\n", fname, got);
            }
        }
    } else {
        send_all(c, "UNKNOWN\n", 8);
    }

    close(c);
    close(srv);
    printf("Connection closed.\n");
    return 0;
}
