#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h> // 添加这一行
#include <sys/stat.h> // 添加这一行

#define PORT 9999
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

void send_file(int client_fd, const char *filename) {
    FILE *fp;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    // 打开要发送的文件
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("File open failed");
        return;
    }

    // 从文件中读取数据并发送给客户端
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        if (send(client_fd, buffer, bytes_read, 0) != bytes_read) {
            perror("Send failed");
            break;
        }
    }

    // 关闭文件
    fclose(fp);
}

int main() {
    int server_fd, client_fd, max_fd, activity, i, valread;
    int client_sockets[MAX_CLIENTS] = {0};
    fd_set readfds;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    char buffer[BUFFER_SIZE];

    // 创建服务器套接字
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 设置服务器套接字选项，启用地址复用
    int enable_reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(enable_reuse)) < 0) {
        perror("Setsockopt SO_REUSEADDR failed");
        exit(EXIT_FAILURE);
    }

    // 设置服务器套接字选项，启用端口复用
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &enable_reuse, sizeof(enable_reuse)) < 0) {
        perror("Setsockopt SO_REUSEPORT failed");
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

    // 清空文件描述符集合
    FD_ZERO(&readfds);

    // 添加服务器套接字到文件描述符集合
    FD_SET(server_fd, &readfds);
    max_fd = server_fd;

    while (1) {
        // 复制文件描述符集合，因为 select() 会修改它
        fd_set temp_fds = readfds;

        // 使用 select() 监视可读事件
        activity = select(max_fd + 1, &temp_fds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("Select failed");
            exit(EXIT_FAILURE);
        }

        // 如果服务器套接字准备好接收连接
        if (FD_ISSET(server_fd, &temp_fds)) {
            // 接受客户端连接
            client_addr_len = sizeof(client_addr);
            if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }

            printf("Client connected from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            // 将新的客户端套接字添加到数组中
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = client_fd;
                    break;
                }
            }

            // 添加新的客户端套接字到文件描述符集合
            FD_SET(client_fd, &readfds);

            // 更新最大文件描述符数
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
        }

        // 处理客户端请求
        for (i = 0; i < MAX_CLIENTS; i++) {
            client_fd = client_sockets[i];

            if (FD_ISSET(client_fd, &temp_fds)) {
                // 接收客户端数据
                if ((valread = read(client_fd, buffer, BUFFER_SIZE)) == 0) {
                    // 客户端断开连接
                    printf("Client disconnected\n");
                    close(client_fd);
                    FD_CLR(client_fd, &readfds);
                    client_sockets[i] = 0;
                } else {
                    // 处理客户端数据
                    buffer[valread] = '\0';
                    printf("Received message from client: %s\n", buffer);

                    // 如果客户端请求下载文件
                    if (strncmp(buffer, "DOWNLOAD", 8) == 0) {
                        // 获取客户端请求的文件名
                        char *filename = buffer + 9; // 假设消息格式为 "DOWNLOAD filename"

                        // 检查文件是否存在
                        if (access(filename, F_OK) != -1) {
                            // 发送文件给客户端
                            send_file(client_fd, filename);
                        } else {
                            // 告知客户端文件不存在
                            send(client_fd, "FILE_NOT_FOUND", 14, 0);
                        }
                    }
                }
            }
        }
    }

    // 关闭服务器套接字
    close(server_fd);

    return 0;
}
