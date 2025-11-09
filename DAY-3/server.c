#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

#define PORT 8080

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    DIR *dir;
    struct dirent *entry;
    char buffer[1024];
    char filename[256];
    char filepath[512];

    // Check share directory
    dir = opendir("share");
    if(!dir){
        printf("ERROR: share/ directory missing.\n");
        return 1;
    }

    // Create socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock < 0){
        perror("Socket creation failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Bind failed");
        exit(1);
    }

    listen(server_sock, 5);
    printf("\n[DAY-3 SERVER] Waiting for client...\n");

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
    printf("Client connected.\n");

    // Let client know share dir is OK
    write(client_sock, "OKAY", 4);

    // Send file list
    rewinddir(dir);
    while((entry = readdir(dir)) != NULL){
        if(entry->d_type == DT_REG){
            snprintf(buffer, sizeof(buffer), "%s\n", entry->d_name);
            write(client_sock, buffer, strlen(buffer));
        }
    }
    write(client_sock, "END\n", 4);

    // Receive filename
    memset(filename, 0, sizeof(filename));
    int n = read(client_sock, filename, sizeof(filename)-1);
    filename[n] = '\0';

    // Remove newline
    filename[strcspn(filename, "\n")] = 0;
    filename[strcspn(filename, "\r")] = 0;

    // Construct file path
    snprintf(filepath, sizeof(filepath), "share/%s", filename);

    FILE *fp = fopen(filepath, "rb");
    if(!fp){
        write(client_sock, "NOFILE", 6);
        close(client_sock);
        close(server_sock);
        return 0;
    }

    // Send file exists response
    write(client_sock, "FOUND", 5);

    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    sprintf(buffer, "%ld", size);
    write(client_sock, buffer, strlen(buffer));

    // Send file data
    while((n = fread(buffer, 1, sizeof(buffer), fp)) > 0){
        write(client_sock, buffer, n);
    }

    fclose(fp);
    close(client_sock);
    close(server_sock);

    printf("File transfer completed.\n");
    return 0;
}
