// Day-5/client.c
// Authenticate, then send XOR-encoded commands (LIST/GET/PUT)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8080
#define BUFSZ 8192

ssize_t send_all(int fd, const void *buf, size_t len) {
    size_t sent = 0; const char *b = buf;
    while (sent < len) {
        ssize_t s = write(fd, b + sent, len - sent);
        if (s <= 0) return -1;
        sent += s;
    }
    return sent;
}
ssize_t recv_line_plain(int fd, char *out, size_t maxlen) {
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
void xor_buf(unsigned char *buf, size_t len, const unsigned char *key, size_t klen) {
    for (size_t i = 0; i < len; ++i) buf[i] ^= key[i % klen];
}

int main(void) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }
    struct sockaddr_in serv = {0}; serv.sin_family = AF_INET; serv.sin_port = htons(PORT); serv.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(s, (struct sockaddr*)&serv, sizeof(serv)) < 0) { perror("connect"); close(s); return 1; }

    // AUTH step
    char user[128], pass[128];
    printf("Username: "); if (!fgets(user, sizeof(user), stdin)) return 1; user[strcspn(user, "\n")] = 0;
    printf("Password: "); if (!fgets(pass, sizeof(pass), stdin)) return 1; pass[strcspn(pass, "\n")] = 0;
    char auth[300]; snprintf(auth, sizeof(auth), "AUTH %s %s\n", user, pass);
    send_all(s, auth, strlen(auth));
    char resp[BUFSZ];
    if (recv_line_plain(s, resp, sizeof(resp)) <= 0) { fprintf(stderr,"no auth_resp\n"); close(s); return 1; }
    if (strncmp(resp, "AUTH_OK", 7) != 0) { printf("Auth failed: %s\n", resp); close(s); return 1; }
    printf("Authenticated OK\n");

    size_t klen = strlen(pass);
    unsigned char key[256]; memcpy(key, pass, klen);

    // choose action
    printf("Choose action (1) LIST  (2) GET  (3) PUT : ");
    int ch = 0; if (scanf("%d%*c", &ch) != 1) { close(s); return 1; }

    if (ch == 1) {
        // send "LIST\n" XORed
        char cmd[] = "LIST\n";
        xor_buf((unsigned char*)cmd, strlen(cmd), key, klen);
        send_all(s, cmd, strlen(cmd));
        // read XORed lines until "END\n"
        while (1) {
            // read until '\n' raw byte, then xor and display
            unsigned char cch;
            size_t idx = 0;
            char line[1024];
            while (1) {
                ssize_t r = read(s, &cch, 1);
                if (r <= 0) { fprintf(stderr,"read fail\n"); close(s); return 1; }
                unsigned char dec = cch ^ key[idx % klen];
                line[idx++] = dec;
                if (dec == '\n' || idx+1 >= sizeof(line)) break;
            }
            line[idx] = 0;
            if (strcmp(line, "END\n") == 0) break;
            printf("%s", line);
        }
    } else if (ch == 2) {
        char fname[512];
        printf("Enter filename to GET: ");
        if (!fgets(fname, sizeof(fname), stdin)) { close(s); return 1; }
        fname[strcspn(fname, "\n")] = 0;
        char cmd[600]; snprintf(cmd, sizeof(cmd), "GET %s\n", fname);
        xor_buf((unsigned char*)cmd, strlen(cmd), key, klen);
        send_all(s, cmd, strlen(cmd));
        // read XORed response line (e.g. FILEOK\n or NOFILE\n)
        // we'll read raw bytes until newline, XOR them
        unsigned char cch; size_t idx = 0; char line[256];
        while (1) {
            ssize_t r = read(s, &cch, 1);
            if (r <= 0) { fprintf(stderr,"readfail\n"); close(s); return 1; }
            unsigned char dec = cch ^ key[idx % klen];
            line[idx++] = dec;
            if (dec == '\n' || idx+1 >= sizeof(line)) break;
        }
        line[idx] = 0;
        if (strcmp(line, "NOFILE\n") == 0) { printf("Server: NOFILE\n"); close(s); return 0; }
        if (strncmp(line, "FILEOK\n", 7) != 0) { printf("Unexpected: %s\n", line); close(s); return 1; }
        // next read size line XORed
        idx = 0;
        while (1) {
            ssize_t r = read(s, &cch, 1);
            if (r <= 0) break;
            unsigned char dec = cch ^ key[idx % klen];
            line[idx++] = dec;
            if (dec == '\n' || idx+1 >= sizeof(line)) break;
        }
        line[idx] = 0;
        long size = atol(line);
        printf("Receiving %ld bytes (XOR-decoded)...\n", size);
        int fd = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) { perror("open"); close(s); return 1; }
        long got = 0;
        unsigned char buf[BUFSZ];
        while (got < size) {
            ssize_t need = (size - got) > BUFSZ ? BUFSZ : (size - got);
            ssize_t r = read(s, buf, need);
            if (r <= 0) break;
            xor_buf(buf, r, key, klen);
            write(fd, buf, r);
            got += r;
        }
        close(fd);
        printf("Saved '%s' (%ld bytes)\n", fname, got);
    } else if (ch == 3) {
        char fname[512];
        printf("Enter local filename to PUT: ");
        if (!fgets(fname, sizeof(fname), stdin)) { close(s); return 1; }
        fname[strcspn(fname, "\n")] = 0;
        struct stat st;
        if (stat(fname, &st) < 0) { perror("stat"); close(s); return 1; }
        char cmd[600]; snprintf(cmd, sizeof(cmd), "PUT %s\n", fname);
        xor_buf((unsigned char*)cmd, strlen(cmd), key, klen);
        send_all(s, cmd, strlen(cmd));
        // send size line XORed
        char meta[64]; snprintf(meta, sizeof(meta), "%ld\n", (long)st.st_size);
        xor_buf((unsigned char*)meta, strlen(meta), key, klen);
        send_all(s, meta, strlen(meta));
        // send file XORed in chunks
        int fd = open(fname, O_RDONLY);
        if (fd < 0) { perror("open"); close(s); return 1; }
        unsigned char buf[BUFSZ];
        ssize_t r;
        while ((r = read(fd, buf, sizeof(buf))) > 0) {
            xor_buf(buf, r, key, klen);
            if (send_all(s, buf, r) < 0) { perror("send"); break; }
        }
        close(fd);
        // read server response line (XORed)
        unsigned char cch; size_t idx2 = 0; char line2[64];
        while (1) {
            ssize_t rr = read(s, &cch, 1);
            if (rr <= 0) break;
            unsigned char dec = cch ^ key[idx2 % klen];
            line2[idx2++] = dec;
            if (dec == '\n' || idx2+1 >= sizeof(line2)) break;
        }
        line2[idx2] = 0;
        printf("Server: %s", line2);
    } else {
        printf("Invalid selection\n");
    }

    close(s);
    return 0;
}
