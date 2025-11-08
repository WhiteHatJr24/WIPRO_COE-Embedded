#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080

int main(){
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char filename[256], buffer[1024];

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
    printf("Server started. Waiting for client...\n");

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
    printf("Client connected.\n");

    // Send file list
    DIR *d = opendir("./share");
    if(!d) {
        write(client_sock, "ERROR", 5);
        perror("Cannot open 'share' directory");
        exit(1);
    } else {
        write(client_sock, "OKAY", 4);
    }

    struct dirent *dir;
    while((dir = readdir(d)) != NULL){
        if(dir->d_type == DT_REG){
            write(client_sock, dir->d_name, strlen(dir->d_name));
            write(client_sock, "\n", 1);
        }
    }
    write(client_sock, "END", 3);
    closedir(d);

    // Receive filename request
    read(client_sock, filename, sizeof(filename));
    filename[strcspn(filename, "\n")] = 0;

    char path[300];
    snprintf(path, sizeof(path), "./share/%s", filename);

    FILE *fp = fopen(path, "rb");
    if(!fp){
        write(client_sock, "NOFILE", 6);
        printf("Requested file not found.\n");
    } else {
        write(client_sock, "FILEOK", 6);
        size_t n;
        while((n = fread(buffer, 1, sizeof(buffer), fp)) > 0){
            write(client_sock, buffer, n);
        }
        fclose(fp);
        printf("File sent: %s\n", filename);
    }

    close(client_sock);
    close(server_sock);
    return 0;
}
