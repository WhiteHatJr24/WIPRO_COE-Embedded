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

    // Check server share directory availability
    read(sock, buffer, 4);
    if(strncmp(buffer, "OKAY", 4) != 0){
        printf("Server cannot access share directory.\n");
        close(sock);
        exit(1);
    }

    // Receive file list
    printf("\nAvailable files on server:\n");
    while(1){
        int i = 0;
        char ch;

        while(read(sock, &ch, 1) == 1 && ch != '\n'){
            buffer[i++] = ch;
        }
        buffer[i] = '\0';

        if(strcmp(buffer, "END") == 0)
            break;

        printf(" - %s\n", buffer);
    }

    // Ask filename
    printf("\nEnter filename to download: ");
    fgets(filename, sizeof(filename), stdin);
    filename[strcspn(filename, "\n")] = '\0';

    write(sock, filename, strlen(filename));

    // Check file existence
    read(sock, buffer, 6);
    if(strncmp(buffer, "NOFILE", 6) == 0){
        printf("File not found on server.\n");
        close(sock);
        exit(1);
    }

    // Receive file size
    int n = read(sock, buffer, sizeof(buffer)-1);
    buffer[n] = '\0';
    long file_size = atol(buffer);

    printf("\nFile Size: %ld bytes\n", file_size);

    FILE *fp = fopen(filename, "wb");
    if(!fp){
        perror("File creation failed");
        close(sock);
        exit(1);
    }

    long received = 0;
    while(received < file_size){
        n = read(sock, buffer, sizeof(buffer));
        if(n <= 0) break; // Stop if connection ends unexpectedly
        fwrite(buffer, 1, n, fp);
        received += n;
    }

    fclose(fp);
    close(sock);

    printf("\n File downloaded successfully. Program ended.\n\n");
    return 0;
}
