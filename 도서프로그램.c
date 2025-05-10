


// 도서관리 시스템 - 서버코드 최종 수정본
// 작성자: 김성연

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <process.h>

#define BUF_SIZE 1024
#define MAX_CLNT 100
#define MAX_BOOKS 700
#define MAX_USERS 100

typedef struct {
    int id;
    char title[100];
    char author[100];
    float rating;
} Book;

typedef struct {
    char id[50];
    char pw[50];
} User;

Book books[MAX_BOOKS];
int book_count = 0;
User users[MAX_USERS];
int user_count = 0;

SOCKET clientSocks[MAX_CLNT];
int clientCount = 0;
HANDLE hMutex;

unsigned WINAPI HandleClient(void* arg);
void ErrorHandling(const char* msg);
void load_books();
void save_books();
void load_users();
void save_users();
int check_login(const char* id, const char* pw);
void sort_books_by_rating();

int main() {
    WSADATA wsaData;
    SOCKET serverSock, clientSock;
    SOCKADDR_IN serverAddr, clientAddr;
    int clientAddrSize;
    HANDLE hThread;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");

    hMutex = CreateMutex(NULL, FALSE, NULL);
    serverSock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(12345);

    if (bind(serverSock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        ErrorHandling("bind() error");
    if (listen(serverSock, 5) == SOCKET_ERROR)
        ErrorHandling("listen() error");

    printf("Server is running...\n");

    load_books();
    load_users();

    while (1) {
        clientAddrSize = sizeof(clientAddr);
        clientSock = accept(serverSock, (SOCKADDR*)&clientAddr, &clientAddrSize);

        WaitForSingleObject(hMutex, INFINITE);
        clientSocks[clientCount++] = clientSock;
        ReleaseMutex(hMutex);

        hThread = (HANDLE)_beginthreadex(NULL, 0, HandleClient, (void*)&clientSock, 0, NULL);
        printf("Connected client: %s\n", inet_ntoa(clientAddr.sin_addr));
    }

    closesocket(serverSock);
    WSACleanup();
    return 0;
}

unsigned WINAPI HandleClient(void* arg) {
    SOCKET clientSock = *((SOCKET*)arg);
    char msg[BUF_SIZE];
    int strLen;

    while ((strLen = recv(clientSock, msg, sizeof(msg) - 1, 0)) > 0) {
        msg[strLen] = '\\0';

        if (strncmp(msg, "lo/", 3) == 0) {
            char* id = strtok(msg + 3, "/");
            char* pw = strtok(NULL, "/");
            if (check_login(id, pw)) {
                send(clientSock, "LOGIN_SUCCESS", strlen("LOGIN_SUCCESS"), 0);
            } else {
                send(clientSock, "LOGIN_FAIL", strlen("LOGIN_FAIL"), 0);
            }
        } else if (strncmp(msg, "0/", 2) == 0) {
            char* keyword = msg + 2;
            char result[BUF_SIZE * 10] = "";
            for (int i = 0; i < book_count; i++) {
                if (strstr(books[i].title, keyword) || strstr(books[i].author, keyword)) {
                    char line[BUF_SIZE];
                    sprintf(line, "%d\\t%s\\t%s\\t%.1f\\n", books[i].id, books[i].title, books[i].author, books[i].rating);
                    strcat(result, line);
                }
            }
            if (strlen(result) == 0)
                strcpy(result, "검색 결과 없음\\n");
            send(clientSock, result, strlen(result), 0);
        } else if (strncmp(msg, "1/", 2) == 0) {
            int id = atoi(strtok(msg + 2, "/"));
            char* title = strtok(NULL, "/");
            char* author = strtok(NULL, "/");
            float rating = atof(strtok(NULL, "/"));
            Book b = { id, "", "", rating };
            strcpy(b.title, title);
            strcpy(b.author, author);
            books[book_count++] = b;
            save_books();
            send(clientSock, "ADD_SUCCESS", strlen("ADD_SUCCESS"), 0);
        } else if (strncmp(msg, "2/", 2) == 0) {
            int id = atoi(msg + 2);
            int found = 0;
            for (int i = 0; i < book_count; i++) {
                if (books[i].id == id) {
                    for (int j = i; j < book_count - 1; j++)
                        books[j] = books[j + 1];
                    book_count--;
                    found = 1;
                    break;
                }
            }
            save_books();
            send(clientSock, found ? "DELETE_SUCCESS" : "DELETE_FAIL", strlen(found ? "DELETE_SUCCESS" : "DELETE_FAIL"), 0);
        } else if (strncmp(msg, "3/", 2) == 0) {
            int id = atoi(strtok(msg + 2, "/"));
            char* title = strtok(NULL, "/");
            char* author = strtok(NULL, "/");
            float rating = atof(strtok(NULL, "/"));
            int found = 0;
            for (int i = 0; i < book_count; i++) {
                if (books[i].id == id) {
                    strcpy(books[i].title, title);
                    strcpy(books[i].author, author);
                    books[i].rating = rating;
                    found = 1;
                    break;
                }
            }
            save_books();
            send(clientSock, found ? "EDIT_SUCCESS" : "EDIT_FAIL", strlen(found ? "EDIT_SUCCESS" : "EDIT_FAIL"), 0);
        } else if (strcmp(msg, "4") == 0) {
            sort_books_by_rating();
            char result[BUF_SIZE * 10] = "";
            for (int i = 0; i < book_count; i++) {
                char line[BUF_SIZE];
                sprintf(line, "%d\\t%s\\t%s\\t%.1f\\n", books[i].id, books[i].title, books[i].author, books[i].rating);
                strcat(result, line);
            }
            send(clientSock, result, strlen(result), 0);
        }
    }

    closesocket(clientSock);
    return 0;
}

void sort_books_by_rating() {
    for (int i = 0; i < book_count - 1; i++) {
        for (int j = i + 1; j < book_count; j++) {
            if (books[i].rating < books[j].rating) {
                Book temp = books[i];
                books[i] = books[j];
                books[j] = temp;
            }
        }
    }
}

void load_books() {
    FILE* fp = fopen("books.txt", "r");
    if (!fp) {
        perror("books.txt 열기 실패 (같은 폴더에 있어야 함)");
        exit(1);
    }
    while (fscanf(fp, "%d\\t%[^\t]\\t%[^\t]\\t%f", &books[book_count].id, books[book_count].title, books[book_count].author, &books[book_count].rating) != EOF) {
        book_count++;
    }
    fclose(fp);
}

void save_books() {
    FILE* fp = fopen("books.txt", "w");
    if (!fp) {
        perror("books.txt 저장 실패");
        return;
    }
    for (int i = 0; i < book_count; i++) {
        fprintf(fp, "%d\\t%s\\t%s\\t%.1f\\n", books[i].id, books[i].title, books[i].author, books[i].rating);
    }
    fclose(fp);
}

void load_users() {
    FILE* fp = fopen("users.txt", "r");
    if (!fp) {
        perror("users.txt 열기 실패 (같은 폴더에 있어야 함)");
        exit(1);
    }
    while (fscanf(fp, "%s %s", users[user_count].id, users[user_count].pw) != EOF) {
        user_count++;
    }
    fclose(fp);
}

void save_users() {
    FILE* fp = fopen("users.txt", "w");
    if (!fp) return;
    for (int i = 0; i < user_count; i++) {
        fprintf(fp, "%s %s\\n", users[i].id, users[i].pw);
    }
    fclose(fp);
}

int check_login(const char* id, const char* pw) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].id, id) == 0 && strcmp(users[i].pw, pw) == 0)
            return 1;
    }
    return 0;
}

void ErrorHandling(const char* msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}


