#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main(){
    int sock;
    struct sockaddr_in server_addr;
    char buffer[1024], filename[256];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        perror("Socket creation failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Connection failed");
        exit(1);
    }

    // Check directory status
    read(sock, buffer, 4);
    if(strncmp(buffer, "OKAY", 4) != 0){
        printf("ERROR: Server cannot access share directory.\n");
        exit(1);
    }

    printf("\nAvailable files on server:\n");
    while(1){
        int n = read(sock, buffer, sizeof(buffer));
        if(n <= 0) break;
        buffer[n] = '\0';

        if(strcmp(buffer, "END") == 0) break;
        printf(" - %s", buffer);
    }

    printf("\nEnter filename to download: ");
    fgets(filename, sizeof(filename), stdin);
    write(sock, filename, strlen(filename));

    read(sock, buffer, 6);
    if(strncmp(buffer, "NOFILE", 6) == 0){
        printf("File not found on server.\n");
        exit(1);
    }

    FILE *fp = fopen(filename, "wb");
    int n;
    while((n = read(sock, buffer, sizeof(buffer))) > 0){
        fwrite(buffer, 1, n, fp);
        if(n < sizeof(buffer)) break;
    }
    fclose(fp);

    printf("File downloaded successfully.\n");
    close(sock);
    return 0;
}
