#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define BUF_SIZE 100

void ErrorHandling(char* msg);

int main() {
    WSADATA wsaData;
    SOCKET sock;
    SOCKADDR_IN servAddr;
    char msg[BUF_SIZE];
    int strLen;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        ErrorHandling("socket() error");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr("220.82.10.223"); // 서버 IP 고정
    servAddr.sin_port = htons(12345); // 서버 포트

    if (connect(sock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
        ErrorHandling("connect() error!");

    printf("Connected to server...\n");

    while (1) {
        printf("Input message (q to quit): ");
        fgets(msg, BUF_SIZE, stdin);

        if (!strcmp(msg, "q\n")) break;

        send(sock, msg, strlen(msg), 0);
        strLen = recv(sock, msg, BUF_SIZE - 1, 0);
        msg[strLen] = '\0';
        printf("Message from server: %s\n", msg);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}

void ErrorHandling(char* msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}