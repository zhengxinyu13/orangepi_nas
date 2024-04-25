#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "192.168.31.72"
#define PORT 9999
#define BUFFER_SIZE 1024

int main()
{
    int client_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // 创建套接字
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址结构
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    // 连接服务器
    // yangzihan add -> connect会导致阻塞，采用定时器超时唤醒解决阻塞时间过长的问题
    // yangzihan add -> 或者优化为连接线程，把连接这种阻塞的动作切异步出去，先让后面的fgets运行起来，fgets后等信号量
    // yangzihan add -> 连接线程里面连接成功通知信号给这里的等待。这样fgets就不会被connect阻塞住起不来。不然connect没连上用户一直看不到fgets
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server\n");

    // 用户选择上传文件或者下载文件
    printf("Enter 'UPLOAD' to upload file or 'DOWNLOAD' to download file: ");
    fgets(buffer, BUFFER_SIZE, stdin);
    buffer[strcspn(buffer, "\n")] = '\0'; // 去除换行符

    // 发送选择到服务器
    // yangzihan add -> 所有的send和recv都不应该阻塞套接字，接收发送要设置NON_WAIT
    // yangzihan add -> 你想写10字节，套接字可能只有8字节空闲，剩下2字节写不进去要while(ret != 0)循环写进去
    if (send(client_fd, buffer, strlen(buffer), 0) == -1) {
        perror("Send failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    if (strcmp(buffer, "UPLOAD") == 0) {
        // 上传文件
        printf("Enter the path of the file to upload: ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // 去除换行符

        FILE *file = fopen(buffer, "rb");
        if (file == NULL) {
            perror("File opening failed");
            close(client_fd);
            exit(EXIT_FAILURE);
        }

        printf("Uploading file to server...\n");

        // 从文件读取数据并发送到服务器
        ssize_t bytes_read;
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            if (send(client_fd, buffer, bytes_read, 0) == -1) {
                perror("Send failed");
                fclose(file);
                close(client_fd);
                exit(EXIT_FAILURE);
            }
        }

        fclose(file);
        printf("File uploaded successfully\n");
    } else if (strcmp(buffer, "DOWNLOAD") == 0) {
        // 下载文件
        FILE *file = fopen("received_file", "wb");
        if (file == NULL) {
            perror("File opening failed");
            close(client_fd);
            exit(EXIT_FAILURE);
        }

        printf("Downloading file from server...\n");

        // 接收服务器发送的文件数据并写入文件
        ssize_t bytes_received;
        while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
            fwrite(buffer, 1, bytes_received, file);
        }

        fclose(file);
        printf("File downloaded and saved as 'received_file'\n");
    }

    // 关闭套接字
    close(client_fd);

    return 0;
}

