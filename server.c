#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8888
#define BUFFER_SIZE 1024

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];

    // 读取客户端请求
    ssize_t bytes_received;
    if ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) == -1) {
        perror("Receive failed");
        close(client_fd);
        return;
    }

    buffer[bytes_received] = '\0';

    // 如果客户端请求上传文件
    if (strcmp(buffer, "UPLOAD") == 0) {
        FILE *file = fopen("received_file", "wb");
        if (file == NULL) {
            perror("File opening failed");
            close(client_fd);
            return;
        }

        printf("Receiving file from client...\n");

        // 接收文件数据并写入文件
        while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
            fwrite(buffer, 1, bytes_received, file);
        }

        fclose(file);
        printf("File received and saved as 'received_file'\n");
    }
    // 如果客户端请求下载文件
    else if (strcmp(buffer, "DOWNLOAD") == 0) {
        FILE *file = fopen("file_to_send", "rb");
        if (file == NULL) {
            perror("File opening failed");
            close(client_fd);
            return;
        }

        printf("Sending file to client...\n");

        // 从文件读取数据并发送给客户端
        while ((bytes_received = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            if (send(client_fd, buffer, bytes_received, 0) == -1) {
                perror("Send failed");
                fclose(file);
                close(client_fd);
                return;
            }
        }

        fclose(file);
        printf("File sent to client\n");
    }

    // 关闭与客户端的连接
    close(client_fd);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;

    // 创建套接字
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址结构
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // 绑定套接字
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // 监听连接
    if (listen(server_fd, 5) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        // 接受客户端连接
        client_addr_len = sizeof(client_addr);
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        printf("Client connected from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // 创建子进程处理客户端连接
        pid_t pid = fork();
        if (pid == -1) {
            perror("Fork failed");
            close(client_fd);
            continue;
        } else if (pid == 0) { // 子进程
            close(server_fd); // 关闭不需要的服务器套接字
            handle_client(client_fd); // 处理客户端连接
            exit(EXIT_SUCCESS);
        } else { // 父进程
            close(client_fd); // 关闭不需要的客户端套接字
        }
    }

    // 关闭服务器套接字
    close(server_fd);

    return 0;
}

