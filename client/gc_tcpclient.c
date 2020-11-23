#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("usage: ./gc_tcpclient <HOST>\n");
        return 0;
    }
    struct sockaddr_in seraddr;
    seraddr.sin_family = AF_INET;
    seraddr.sin_port = htons(8080);
    inet_pton(AF_INET, argv[1], &seraddr.sin_addr.s_addr);
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    printf("try connecting to server...\n");
    if (connect(sockfd, (struct sockaddr*)&seraddr, (socklen_t)sizeof(struct sockaddr_in)) < 0) {
        printf("failed to connect to server, %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }
    printf("connected to server!\n");
    char buff[4096];
    ssize_t n = 0;
    while ((n = read(0, buff, sizeof(buff))) > 0)  {
        write(sockfd, buff, n - 1);
        n = read(sockfd, buff, n);
        if (n == 0) {
            printf("server close connection\n");
            close(sockfd);
            return  0;
        }
        buff[n] = '\0';
        printf("sever echo: %s\n", buff);
    }
    if (n == 0) {
        close(sockfd);
        printf("client close connection\n");
    } else {
        close(sockfd);
        printf("%s", strerror(errno));
    }
    return 0;
}
