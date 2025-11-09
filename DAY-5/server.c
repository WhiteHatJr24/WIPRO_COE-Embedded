// Day-5/server.c
// Simple AUTH + XOR demo. After successful AUTH, client sends commands encoded with XOR using password as key.
// Server supports LIST, GET, PUT after auth.
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
#define USERS_FILE "./users.txt"
#define BUFSZ 8192

// helpers
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
    size_t sent = 0; const char *b = buf;
    while (sent < len) {
        ssize_t s = write(fd, b + sent, len - sent);
        if (s <= 0) return -1;
        sent += s;
    }
    return sent;
}
void xor_buf(unsigned char *buf, size_t len, const unsigned char *key, size_t klen) {
    for (size_t i = 0; i < len; ++i) buf[i] ^= key[i % klen];
}

int check_user(const char *user, const char *pass) {
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) return 0;
    char line[512];
    int ok = 0;
    while (fgets(line, sizeof(line), f)) {
        char *p = strchr(line, '\n'); if (p) *p = 0;
        char *sep = strchr(line, ':'); if (!sep) continue;
        *sep = 0;
        if (strcmp(line, user) == 0 && strcmp(sep+1, pass) == 0) { ok = 1; break; }
    }
    fclose(f);
    return ok;
}

int main(void) {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }
    int opt = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {0}; addr.sin_family = AF_INET; addr.sin_port = htons(PORT); addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(srv); return 1; }
    if (listen(srv, 5) < 0) { perror("listen"); close(srv); return 1; }
    printf("Day-5 server: listening on %d\n", PORT);

    struct sockaddr_in cli; socklen_t cli_len = sizeof(cli);
    int c = accept(srv, (struct sockaddr*)&cli, &cli_len);
    if (c < 0) { perror("accept"); close(srv); return 1; }
    printf("Client connected\n");

    // Expect AUTH user pass\n (plaintext)
    char line[BUFSZ];
    if (recv_line(c, line, sizeof(line)) <= 0) { fprintf(stderr,"no auth\n"); close(c); close(srv); return 1; }
    line[strcspn(line, "\n")] = 0;
    if (strncmp(line, "AUTH ", 5) != 0) { send_all(c, "AUTH_FAIL\n", 10); close(c); close(srv); return 1; }
    char user[128], pass[128];
    if (sscanf(line + 5, "%127s %127s", user, pass) != 2) { send_all(c, "AUTH_FAIL\n", 10); close(c); close(srv); return 1; }
    if (!check_user(user, pass)) { send_all(c, "AUTH_FAIL\n", 10); close(c); close(srv); return 1; }
    // ok
    send_all(c, "AUTH_OK\n", 8);
    size_t klen = strlen(pass);
    unsigned char key[256];
    memcpy(key, pass, klen);

    // Now next command comes XOR-encoded by client using password as key.
    // Read a line (XORed) by reading until '\n' raw bytes and then XORing in-place.
    // For simplicity, we'll read bytes one by one until '\n' then decode.
    // WARNING: This is demo-level only.
    size_t idx = 0;
    // read XORed command line
    while (1) {
        unsigned char ch;
        ssize_t r = read(c, &ch, 1);
        if (r <= 0) { fprintf(stderr, "read fail\n"); close(c); close(srv); return 1; }
        ch ^= key[idx % klen];
        line[idx++] = ch;
        if (ch == '\n' || idx + 1 >= sizeof(line)) break;
    }
    line[idx] = '\0';
    line[strcspn(line, "\n")] = 0;

    if (strncmp(line, "LIST", 4) == 0) {
        // send file list XORed
        DIR *d = opendir(SHARE_DIR);
        if (!d) { char tmp[] = "ERR\n"; xor_buf((unsigned char*)tmp, strlen(tmp), key, klen); send_all(c, tmp, strlen(tmp)); close(c); close(srv); return 1; }
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_type == DT_REG) {
                char out[512];
                snprintf(out, sizeof(out), "%s\n", e->d_name);
                xor_buf((unsigned char*)out, strlen(out), key, klen);
                send_all(c, out, strlen(out));
            }
        }
        closedir(d);
        char end[] = "END\n"; xor_buf((unsigned char*)end, strlen(end), key, klen); send_all(c, end, strlen(end));
    } else if (strncmp(line, "GET ", 4) == 0) {
        char *fname = line + 4;
        char path[1024]; snprintf(path, sizeof(path), "%s/%s", SHARE_DIR, fname);
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            char tmp[] = "NOFILE\n"; xor_buf((unsigned char*)tmp, strlen(tmp), key, klen); send_all(c, tmp, strlen(tmp));
        } else {
            struct stat st; fstat(fd,&st);
            // send FILEOK\n<size>\n both XORed
            char meta[128];
            snprintf(meta, sizeof(meta), "FILEOK\n%ld\n", (long)st.st_size);
            xor_buf((unsigned char*)meta, strlen(meta), key, klen);
            send_all(c, meta, strlen(meta));
            // send file data XORed in chunks
            unsigned char buf[BUFSZ];
            ssize_t r;
            while ((r = read(fd, buf, sizeof(buf))) > 0) {
                xor_buf(buf, r, key, klen);
                if (send_all(c, buf, r) < 0) break;
            }
            close(fd);
        }
    } else if (strncmp(line, "PUT ", 4) == 0) {
        char *fname = line + 4;
        // read next line (size) XORed
        idx = 0;
        while (1) {
            unsigned char ch;
            ssize_t r = read(c, &ch, 1);
            if (r <= 0) { fprintf(stderr, "read fail\n"); close(c); close(srv); return 1; }
            ch ^= key[idx % klen];
            line[idx++] = ch;
            if (ch == '\n' || idx + 1 >= sizeof(line)) break;
        }
        line[idx] = 0;
        long size = atol(line);
        char path[1024]; snprintf(path, sizeof(path), "%s/%s", SHARE_DIR, fname);
        int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) { char tmp[]="ERR\n"; xor_buf((unsigned char*)tmp, strlen(tmp), key, klen); send_all(c,tmp,strlen(tmp)); }
        else {
            long got = 0;
            unsigned char buf[BUFSZ];
            while (got < size) {
                ssize_t need = (size - got) > BUFSZ ? BUFSZ : (size - got);
                ssize_t r = read(c, buf, need);
                if (r <= 0) break;
                xor_buf(buf, r, key, klen);
                write(fd, buf, r);
                got += r;
            }
            close(fd);
            char ok[] = "OK\n"; xor_buf((unsigned char*)ok, strlen(ok), key, klen); send_all(c, ok, strlen(ok));
        }
    } else {
        char tmp[] = "UNKNOWN\n"; xor_buf((unsigned char*)tmp, strlen(tmp), key, klen); send_all(c, tmp, strlen(tmp));
    }

    close(c); close(srv);
    printf("Day-5 done, connection closed.\n");
    return 0;
}
