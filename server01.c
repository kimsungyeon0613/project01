#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <process.h>
#include <time.h>
#include <ctype.h>

#define BUF_SIZE 1000   // 
#define MAX_CLNT 100 // 최대 접속 자수
#define PORT_NUM 5548 // 포트 번호 
#define MAX_BOOKS 700   // 최대 도서 수
#define USER_FILE "C:\\Coding\\project\\project01\\users.txt"   // 사용자 정보 파일 경로
#define BOOK_FILE "C:\\Coding\\project\\project01\\booklist2.txt"   // 도서 정보 파일 경로

int stristr(const char* haystack, const char* needle) {
    while (*haystack) {
        const char *h = haystack, *n = needle;
        while (*h && *n && tolower(*h) == tolower(*n)) {
            h++; n++;
        }
        if (!*n) return 1;  // 매칭 성공
        haystack++;
    }
    return 0;  // 매칭 실패
}

void ErrorHandling(char* msg); 


int clientCount = 0;  // 클라이언트 수
SOCKET clientSocks[MAX_CLNT];  // 클라이언트 소켓 배열
HANDLE hMutex;  // 스레드 동기화 뮤텍스

typedef struct {
    int num;
    char title[50];
    char author[50];
    float rating;

    }Book;
int book_count = 0;
Book books[MAX_BOOKS];  //구조체 할당



// 사용자 인증 함수
int UserOk(const char *id, const char *pass) {
    FILE *file = fopen(USER_FILE, "r");
    if (!file) {
        printf("Failed to open %s for authentication: %s\n", USER_FILE, strerror(errno));
        return 0;
    }

    char line[256], file_id[50], file_pass[50];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        
        // // 구분자로 먼저 시도
        char* sep = strstr(line, "//");
        if (sep) {
            int id_len = sep - line;
            strncpy(file_id, line, id_len);
            file_id[id_len] = '\0';
            strcpy(file_pass, sep + 2);
        } else {
            // 탭 구분자로 시도
            if (sscanf(line, "%49s\t%49s", file_id, file_pass) != 2) {
                continue;
            }
        }
        
        if (strcmp(id, file_id) == 0 && strcmp(pass, file_pass) == 0) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

int load_books(void) {
    FILE *f = fopen(BOOK_FILE, "r");
    if (!f) {
        perror("fopen");
        return 0;
    }

    book_count = 0;
    char line[BUF_SIZE];

    while (book_count < MAX_BOOKS && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';  // 개행 제거

        // 탭 기준으로 문자열 분리
        char *num_str   = strtok(line, "\t");
        char *title     = strtok(NULL, "\t");
        char *author    = strtok(NULL, "\t");
        char *rating_str= strtok(NULL, "\t");

        // 모두 제대로 분리되었는지 확인
        if (!num_str || !title || !author || !rating_str)
            continue;

        books[book_count].num = atoi(num_str);
        strncpy(books[book_count].title, title, sizeof(books[book_count].title) - 1);
        strncpy(books[book_count].author, author, sizeof(books[book_count].author) - 1);
        books[book_count].rating = atof(rating_str);

        book_count++;
    }

    fclose(f);
    return book_count;
}



void send_books_to_client(SOCKET clientSock) {
    char buf[512];
    for (int i = 0; i < book_count; i++) {
        snprintf(buf, sizeof(buf), "%d\t%s\t%s\t%.2f\n", 
                 books[i].num, books[i].title, books[i].author, books[i].rating);
        send(clientSock, buf, strlen(buf), 0);
    }
    send(clientSock, "[END]\n", 6, 0);  // 전송 완료 표시
}





void search_title_book(SOCKET sock, const char *msg) {
    char buf[BUF_SIZE];

    // msg는 "1/[keyword]" 형식 → 키워드 추출
    const char* keyword = strchr(msg, '/');
    if (!keyword || strlen(keyword) <= 1) {
        printf("검색어 누락 또는 유효하지 않음: %s\n", msg);
        send(sock, "검색어가 없습니다.\n[END]\n", strlen("검색어가 없습니다.\n[END]\n"), 0);
        return;
    }
    keyword++;
    printf("검색어: %s\n", keyword);

    int found = 0;
    for (int i = 0; i < book_count; ++i) {
        if (stristr(books[i].title, keyword) || stristr(books[i].author, keyword)) {
            int len = snprintf(buf, sizeof(buf),
                               "%d\t%s\t%s\t%.2f\n",
                               books[i].num,
                               books[i].title,
                               books[i].author,
                               books[i].rating);
            if (len >= sizeof(buf)) {
                printf("경고: 버퍼 초과, 데이터 잘림 (도서 %d)\n", books[i].num);
            }
            send(sock, buf, len, 0);
            found = 1;
        }
    }

    if (!found) {
        printf("검색 결과: 없음\n");
        send(sock, "검색 결과 없음\n", strlen("검색 결과 없음\n"), 0);
    } else {
        printf("검색 결과: %d건 발견\n", found);
    }

    send(sock, "[END]\n", strlen("[END]\n"), 0);
}

int add_book(SOCKET sock, const char *msg) {
    const char *data = strchr(msg, '/');
    if (!data || strlen(data) <= 1) {
        send(sock, "ADD:FORMAT_ERROR\n", strlen("ADD:FORMAT_ERROR\n"), 0);
        send(sock, "[END]\n", strlen("[END]\n"), 0);
        return -1;
    }
    data++;  // '/' 다음부터 시작

    char temp[BUF_SIZE];
    strncpy(temp, data, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *title = strtok(temp, "\t");
    char *author = strtok(NULL, "\t");
    char *rating_str = strtok(NULL, "\t");

    if (!title || !author || !rating_str) {
        send(sock, "ADD:FORMAT_ERROR\n", strlen("ADD:FORMAT_ERROR\n"), 0);
        send(sock, "[END]\n", strlen("[END]\n"), 0);
        return -2;
    }

    // 개행 문자 제거
    title[strcspn(title, "\n")] = '\0';
    author[strcspn(author, "\n")] = '\0';
    rating_str[strcspn(rating_str, "\n")] = '\0';

    float rating = atof(rating_str);

    // 마지막 도서 번호 확인
    FILE *f = fopen(BOOK_FILE, "r");
    int last_num = 0;
    if (f) {
        char line[BUF_SIZE];
        while (fgets(line, sizeof(line), f)) {
            int temp_num;
            if (sscanf(line, "%d", &temp_num) == 1 && temp_num > last_num) {
                last_num = temp_num;
            }
        }
        fclose(f);
    }

    int new_num = last_num + 1;

    // 파일에 추가
    f = fopen(BOOK_FILE, "a");
    if (!f) {
        send(sock, "ADD:FILE_ERROR\n", strlen("ADD:FILE_ERROR\n"), 0);
        send(sock, "[END]\n", strlen("[END]\n"), 0);
        return -3;
    }

    // 줄바꿈 포함하여 저장
    fprintf(f, "%d\t%s\t%s\t%.2f\n", new_num, title, author, rating);
    fclose(f);

    // 구조체 배열에도 반영
    if (book_count < MAX_BOOKS) {
        books[book_count].num = new_num;
        strncpy(books[book_count].title, title, sizeof(books[book_count].title) - 1);
        books[book_count].title[sizeof(books[book_count].title) - 1] = '\0';

        strncpy(books[book_count].author, author, sizeof(books[book_count].author) - 1);
        books[book_count].author[sizeof(books[book_count].author) - 1] = '\0';

        books[book_count].rating = rating;
        book_count++;
    }

    send(sock, "ADD:OK\n", strlen("ADD:OK\n"), 0);
    send(sock, "[END]\n", strlen("[END]\n"), 0);
    return 0;
}


int delete_book(SOCKET sock, const char *msg) {
    const char *data = strchr(msg, '/');
    if (!data || strlen(data) <= 1) {
        send(sock, "DELETE:FORMAT_ERROR\n", strlen("DELETE:FORMAT_ERROR\n"), 0);
        send(sock, "[END]\n", strlen("[END]\n"), 0);
        return -1;
    }
    data++;

    char temp[BUF_SIZE];
    strncpy(temp, data, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *num_str = strtok(temp, "\t");
    if (!num_str) {
        send(sock, "DELETE:FORMAT_ERROR\n", strlen("DELETE:FORMAT_ERROR\n"), 0);
        send(sock, "[END]\n", strlen("[END]\n"), 0);
        return -2;
    }

    int targetNum = atoi(num_str);
    int found = 0;

    
    for (int i = 0; i < book_count; ++i) {
        if (books[i].num == targetNum) {
            for (int j = i; j < book_count - 1; ++j) {
                books[j] = books[j + 1];
            }
            book_count--;
            found = 1;
            break;
        }
    }

    if (!found) {
        send(sock, "DELETE:NOT_FOUND\n", strlen("DELETE:NOT_FOUND\n"), 0);
        send(sock, "[END]\n", strlen("[END]\n"), 0);
        return -3;
    }

   
    for (int i = 0; i < book_count; ++i) {
        books[i].num = i + 1;
    }

 
    FILE *f = fopen(BOOK_FILE, "w");
    if (!f) {
        send(sock, "DELETE:FILE_WRITE_ERROR\n", strlen("DELETE:FILE_WRITE_ERROR\n"), 0);
        send(sock, "[END]\n", strlen("[END]\n"), 0);
        return -4;
    }
    for (int i = 0; i < book_count; ++i) {
        fprintf(f, "%d\t%s\t%s\t%.2f\n",
                books[i].num,
                books[i].title,
                books[i].author,
                books[i].rating);
    }
    fclose(f);

    send(sock, "DELETE:OK\n", strlen("DELETE:OK\n"), 0);
    send(sock, "[END]\n", strlen("[END]\n"), 0);
    return 0;
}

int update_book(SOCKET sock, const char *msg) {
    const char *data = strchr(msg, '/');
    if (!data || strlen(data) <= 1) {
        send(sock, "UPDATE:FORMAT_ERROR\n", strlen("UPDATE:FORMAT_ERROR\n"), 0);
        send(sock, "[END]\n", strlen("[END]\n"), 0);
        return -1;
    }
    data++;

    char temp[BUF_SIZE];
    strncpy(temp, data, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *num_str = strtok(temp, "\t");
    char *title = strtok(NULL, "\t");
    char *author = strtok(NULL, "\t");
    char *rating_str = strtok(NULL, "\t");

    if (!num_str || !title || !author || !rating_str) {
        send(sock, "UPDATE:FORMAT_ERROR\n", strlen("UPDATE:FORMAT_ERROR\n"), 0);
        send(sock, "[END]\n", strlen("[END]\n"), 0);
        return -2;
    }

    int targetNum = atoi(num_str);
    float newRating = atof(rating_str);
    int found = 0;

    // 🔸 1. 구조체 배열에서 수정
    for (int i = 0; i < book_count; ++i) {
        if (books[i].num == targetNum) {
            strncpy(books[i].title, title, sizeof(books[i].title) - 1);
            books[i].title[sizeof(books[i].title) - 1] = '\0';

            strncpy(books[i].author, author, sizeof(books[i].author) - 1);
            books[i].author[sizeof(books[i].author) - 1] = '\0';

            books[i].rating = newRating;
            found = 1;
            break;
        }
    }

    if (!found) {
        send(sock, "UPDATE:NOT_FOUND\n", strlen("UPDATE:NOT_FOUND\n"), 0);
        send(sock, "[END]\n", strlen("[END]\n"), 0);
        return -3;
    }

    // 🔸 2. 파일 업데이트
    FILE *f = fopen(BOOK_FILE, "w");
    if (!f) {
        send(sock, "UPDATE:FILE_WRITE_ERROR\n", strlen("UPDATE:FILE_WRITE_ERROR\n"), 0);
        send(sock, "[END]\n", strlen("[END]\n"), 0);
        return -4;
    }

    for (int i = 0; i < book_count; ++i) {
        fprintf(f, "%d\t%s\t%s\t%.2f\n",
                books[i].num,
                books[i].title,
                books[i].author,
                books[i].rating);
    }

    fclose(f);

    send(sock, "UPDATE:OK\n", strlen("UPDATE:OK\n"), 0);
    send(sock, "[END]\n", strlen("[END]\n"), 0);
    return 0;
}

int compare_rating(const void *a, const void *b) {
    const Book *b1 = (const Book *)a;
    const Book *b2 = (const Book *)b;

    // 평점 내림차순 정렬
    if (b1->rating < b2->rating) return 1;
    if (b1->rating > b2->rating) return -1;

    // 평점이 같으면 도서 번호 오름차순 정렬
    if (b1->num < b2->num) return -1;
    if (b1->num > b2->num) return 1;

    return 0;  // 제목 비교는 필요 없다면 이 부분 제거 가능
}

int sort_rating_book(SOCKET sock, const char *msg) {
    load_books();  // 최신 도서 정보를 다시 로드
    qsort(books, book_count, sizeof(Book), compare_rating);
    send_books_to_client(sock);  // 정렬된 결과 전송
    return 0;
}


int add_user(SOCKET sock, const char *msg) {
    // "6/아이디//비밀번호" 형식에서 슬래시 찾기
    const char *data = strchr(msg, '/');
    if (!data || strlen(data) <= 1) {
        send(sock, "ADD_USER:FORMAT_ERROR\n", strlen("ADD_USER:FORMAT_ERROR\n"), 0);
        return -1;
    }
    data++;  // "아이디//비밀번호" 부분으로 이동

    // 복사해서 가공
    char temp[BUF_SIZE];
    strncpy(temp, data, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    // 아이디와 비밀번호 분리 (// 기준)
    char *sep = strstr(temp, "//");
    if (!sep) {
        send(sock, "ADD_USER:FORMAT_ERROR\n", strlen("ADD_USER:FORMAT_ERROR\n"), 0);
        return -2;
    }

    *sep = '\0';           // 아이디 부분 종료
    const char *id = temp;
    const char *pass = sep + 2;  // 비밀번호 부분

    if (strlen(id) == 0 || strlen(pass) == 0) {
        send(sock, "ADD_USER:EMPTY_FIELDS\n", strlen("ADD_USER:EMPTY_FIELDS\n"), 0);
        return -3;
    }

    // 중복 아이디 확인
    FILE *f = fopen(USER_FILE, "r");
    if (!f) {
        send(sock, "ADD_USER:FILE_OPEN_ERROR\n", strlen("ADD_USER:FILE_OPEN_ERROR\n"), 0);
        return -4;
    }

    char line[BUF_SIZE];
    while (fgets(line, sizeof(line), f)) {
        char file_id[50], file_pass[50];
        char *file_sep = strstr(line, "//");
        if (!file_sep) continue;

        *file_sep = '\0';
        strncpy(file_id, line, sizeof(file_id) - 1);
        file_id[sizeof(file_id) - 1] = '\0';

        if (strcmp(file_id, id) == 0) {
            fclose(f);
            send(sock, "ADD_USER:DUPLICATE_ID\n", strlen("ADD_USER:DUPLICATE_ID\n"), 0);
            return -5;
        }
    }
    fclose(f);

    // 추가
    f = fopen(USER_FILE, "a");
    if (!f) {
        send(sock, "ADD_USER:WRITE_ERROR\n", strlen("ADD_USER:WRITE_ERROR\n"), 0);
        return -6;
    }

    fprintf(f, "%s//%s\n", id, pass);
    fclose(f);

    send(sock, "ADD_USER:OK\n", strlen("ADD_USER:OK\n"), 0);
    send(sock, "[END]\n", strlen("[END]\n"), 0);
    return 0;
}

int delete_user(SOCKET sock, const char *msg) {
    const char *data = strchr(msg, '/');
    if (!data || strlen(data) <= 1) {
        send(sock, "DEL_USER:FORMAT_ERROR\n", strlen("DEL_USER:FORMAT_ERROR\n"), 0);
        return -1;
    }
    data++;

    char target_id[50];
    strncpy(target_id, data, sizeof(target_id) - 1);
    target_id[sizeof(target_id) - 1] = '\0';

    FILE *f = fopen(USER_FILE, "r");
    if (!f) {
        send(sock, "DEL_USER:FILE_OPEN_ERROR\n", strlen("DEL_USER:FILE_OPEN_ERROR\n"), 0);
        return -2;
    }

    FILE *tmp = fopen("users_tmp.txt", "w");
    if (!tmp) {
        fclose(f);
        send(sock, "DEL_USER:TEMP_OPEN_ERROR\n", strlen("DEL_USER:TEMP_OPEN_ERROR\n"), 0);
        return -3;
    }

    char line[BUF_SIZE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';  // 개행 제거

        char id[50], pass[50];
        char *sep = strstr(line, "//");
        if (!sep) continue;

        *sep = '\0';
        strcpy(id, line);
        strcpy(pass, sep + 2);

        if (strcmp(id, target_id) == 0) {
            found = 1;
            continue;  // 삭제 대상은 복사 안함
        }

        fprintf(tmp, "%s//%s\n", id, pass);  // 유지할 계정 다시 저장
    }

    fclose(f);
    fclose(tmp);

    if (!found) {
        remove("users_tmp.txt");
        send(sock, "해당 사용자를 찾을 수 없습니다.\n", strlen("해당 사용자를 찾을 수 없습니다.\n"), 0);
        return -4;
    }

    if (remove(USER_FILE) != 0 || rename("users_tmp.txt", USER_FILE) != 0) {
        send(sock, "DEL_USER:REPLACE_ERROR\n", strlen("DEL_USER:REPLACE_ERROR\n"), 0);
        return -5;
    }

    char msgbuf[BUF_SIZE];
    snprintf(msgbuf, sizeof(msgbuf), "%s 아이디가 삭제되었습니다.\n", target_id);
    send(sock, msgbuf, strlen(msgbuf), 0);
    send(sock, "[END]\n", strlen("[END]\n"), 0);
    return 0;
}


unsigned WINAPI HandleClient(void* arg) {
    load_books();  // 도서 목록 로드
    qsort(books, book_count, sizeof(Book), compare_rating);

    SOCKET clientSock = *((SOCKET*)arg);
    int strLen = 0, i;
    char msg[BUF_SIZE];
    int logged_in = 0;
    char user_id[50] = {0};

    while ((strLen = recv(clientSock, msg, sizeof(msg), 0)) != 0) {
        msg[strLen] = '\0';
        printf("수신된 메시지: '%s'\n", msg);  // 디버깅 로그
     
        if (strncmp(msg, "lo/", 3) == 0) {
            char id[50], pass[50];
            char* first_slash = strchr(msg + 3, '/');
            if (first_slash) {
                int id_len = first_slash - (msg + 3);
                strncpy(id, msg + 3, id_len);
                id[id_len] = '\0';
                strcpy(pass, first_slash + 1);

                if (UserOk(id, pass)) {
                    logged_in = 1;
                    strncpy(user_id, id, sizeof(user_id) - 1);
                    send(clientSock, "OK:로그인 성공\n", strlen("OK:로그인 성공\n"), 0);
                    printf("사용자 로그인: %s\n", id);
                    
                    // 도서 정보 파일 전송
                    send_books_to_client(clientSock);
                    // break;  // 로그인 성공하면 로그인 루프 종료
                } else {
                    send(clientSock, "아이디 또는 비밀번호가 일치하지 않습니다.\n", 
                         strlen("아이디 또는 비밀번호가 일치하지 않습니다.\n"), 0);
                    printf("로그인 실패: %s\n", id);
                }
            }
        } else if (logged_in) {
                // 로그인된 사용자의 요청 처리
                if (strncmp(msg, "1/", 2) == 0) {
                    search_title_book(clientSock, msg);
                } else if (strncmp(msg, "2/", 2) == 0) {
                    add_book(clientSock, msg);
                } else if (strncmp(msg, "3/", 2) == 0) {
                    delete_book(clientSock, msg);
                } else if (strncmp(msg, "4/", 2) == 0) {
                    update_book(clientSock, msg);
                } else if (strncmp(msg, "5/", 2) == 0) {
                    sort_rating_book(clientSock, msg);
                } else if (strncmp(msg, "6/", 2) == 0) {
                    add_user(clientSock, msg);
                } else if (strncmp(msg, "7/", 2) == 0) {
                    delete_user(clientSock, msg);
                } else if (strncmp(msg, "8/", 2) == 0) {
                    send(clientSock, "8", 1, 0);
                    break;
                }
        }
    }

    printf("클라이언트 연결 종료: %s\n", user_id[0] ? user_id : "미인증 사용자");

    WaitForSingleObject(hMutex, INFINITE);
    for (i = 0; i < clientCount; i++) {
        if (clientSock == clientSocks[i]) {
            for (int j = i; j < clientCount - 1; j++) {
                clientSocks[j] = clientSocks[j + 1];
            }
            break;
        }
    }
    clientCount--;
    ReleaseMutex(hMutex);
    closesocket(clientSock);
    return 0;
}


int main() {
    SetConsoleOutputCP(CP_UTF8);
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
    serverAddr.sin_port = htons(PORT_NUM);

    if (bind(serverSock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        ErrorHandling("bind() error");
    if (listen(serverSock, 5) == SOCKET_ERROR)
        ErrorHandling("listen() error");

    printf("서버가 시작되었습니다. 포트: %d\n", PORT_NUM);

    while (1) {
        clientAddrSize = sizeof(clientAddr);
        clientSock = accept(serverSock, (SOCKADDR*)&clientAddr, &clientAddrSize);
        WaitForSingleObject(hMutex, INFINITE);
        clientSocks[clientCount++] = clientSock;
        ReleaseMutex(hMutex);
        hThread = (HANDLE)_beginthreadex(NULL, 0, HandleClient, (void*)&clientSock, 0, NULL);
        printf("연결된 클라이언트 IP: %s\n", inet_ntoa(clientAddr.sin_addr));
    }

    closesocket(serverSock);
    WSACleanup();
    return 0;
}







void END_PROGRAM(SOCKET sock) {
    send(sock, "END_PROGRAM:OK\n", strlen("END_PROGRAM:OK\n"), 0);
    exit(0);
}


void ErrorHandling(char* msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
