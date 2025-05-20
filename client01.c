#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <winsock2.h>
#include <process.h>

#define BUF_SIZE 1000
#define NAME_SIZE 100
#define PORT_NUM 5548
#define IP_ADDR "220.82.10.223"

unsigned WINAPI SendMsg(void* arg);
unsigned WINAPI RecvMsg(void* arg);


unsigned WINAPI SendThread(void* arg);
unsigned WINAPI RecvThread(void* arg);

void ErrorHandling(const char* msg);

char name[NAME_SIZE]="[DEFAULT]";
char msg[BUF_SIZE];

int main() {
    char buf[BUF_SIZE];
    int n;
    SetConsoleOutputCP(CP_UTF8);
    WSADATA wsaData;
    SOCKET sock;
    SOCKADDR_IN servAddr;
    char message[BUF_SIZE];
    int strLen;
    char id[50], pass[50];

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        ErrorHandling("socket() error");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family      = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr("220.82.10.223");
    servAddr.sin_port        = htons(PORT_NUM);

    if (connect(sock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
        ErrorHandling("connect() error!");

    printf("서버에 연결되었습니다.\n");
    printf("로그인이 필요합니다.\n");
    /* 로그인 루프 */
    while (1) {
        printf("\n아이디: ");
        fgets(id, sizeof(id), stdin);
        id[strcspn(id, "\n")] = '\0';

        printf("비밀번호: ");
        fgets(pass, sizeof(pass), stdin);
        pass[strcspn(pass, "\n")] = '\0';

        sprintf(message, "lo/%s/%s", id, pass);
        send(sock, message, strlen(message), 0);

       strLen = recv(sock, message, BUF_SIZE - 1, 0);
    if (strLen <= 0) {
        printf("서버 연결이 끊어졌습니다.\n");
        closesocket(sock);
        WSACleanup();
        return 0;
    }
    message[strLen] = '\0';
    printf("서버 응답: '%s'\n", message);  // 디버깅용

    if (strncmp(message, "OK:로그인 성공", 14) == 0) {
        printf("로그인 성공!\n\n도서 목록:\n");
        while ((n = recv(sock, buf, sizeof(buf)-1, 0)) > 0) {
            buf[n] = '\0';
            printf("%s", buf);
            if (strstr(buf, "[END]\n")) break;
        }
        break;  // 로그인 루프 종료
    }
}

    //기능 안내 
    while (1) {
        char keyword[BUF_SIZE];
        printf("\n메뉴 선택(번호 입력)\n"
           "1. 도서 검색\n"
           "2. 도서 추가\n"
           "3. 도서 삭제\n"
           "4. 도서 수정\n"
           "5. 평점 정렬\n"
           "6. 사용자 추가\n"
           "7. 사용자 삭제\n"
           "8. 종료\n> ");
           printf("선택: ");
    fgets(keyword, sizeof(keyword), stdin);
    keyword[strcspn(keyword, "\n")] = '\0';
        
  if (strcmp(keyword, "1") == 0) {
    printf("검색할 도서명을 입력: ");
    fgets(keyword, sizeof(keyword), stdin);
    keyword[strcspn(keyword, "\n")] = '\0';
    snprintf(message, sizeof(message), "1/%s", keyword);
    send(sock, message, strlen(message), 0);
    while ((n = recv(sock, buf, sizeof(buf)-1, 0)) > 0) {
        buf[n] = '\0';
        if (strstr(buf, "[END]\n")) break;
        printf("%s", buf);
    }
    }else if (strcmp(keyword, "2") == 0) {
        char t[BUF_SIZE], a[BUF_SIZE], r[BUF_SIZE];
        printf("추가할 제목: "); fgets(t,BUF_SIZE,stdin);
        printf("추가할 저자: "); fgets(a,BUF_SIZE,stdin);
        printf("추가할 평점: "); fgets(r,BUF_SIZE,stdin);
        t[strcspn(t,"\n")]=a[strcspn(a,"\n")]=r[strcspn(r,"\n")]=0;
        snprintf(message, sizeof(message),
                 "2/%s\t%s\t%s", t, a, r);
        send(sock, message, strlen(message), 0);
        while ((n = recv(sock, buf, sizeof(buf)-1, 0)) > 0) {
                buf[n] = '\0';
                if (strcmp(buf, "[END]\n") == 0) break;
                printf("%s", buf);
            }
      
            printf("도서 추가 완료\n");
        }else if (strcmp(keyword, "3") == 0) {
        printf("삭제할 도서 번호를 입력하세요: ");
        fgets(keyword, sizeof(keyword), stdin);
        keyword[strcspn(keyword, "\n")] = '\0';
        snprintf(message, sizeof(message), "3/%s\t", keyword);
        send(sock, message, strlen(message), 0);
         while ((n = recv(sock, buf, sizeof(buf)-1, 0)) > 0) {
    buf[n] = '\0';
    if (strstr(buf, "[END]\n") != NULL) {
        *strstr(buf, "[END]\n") = '\0';  // [END] 잘라내기
        printf("%s", buf);
        break;
    }
    printf("%s", buf);
}
            printf("도서 삭제 완료\n");
        // 서버 응답 대기…
        }else if(strcmp(keyword, "4") == 0){
            char num[BUF_SIZE], t[BUF_SIZE], a[BUF_SIZE], r[BUF_SIZE];
            printf("수정할 도서 번호 입력: "); fgets(num, BUF_SIZE, stdin);
            printf("새 제목: "); fgets(t, BUF_SIZE, stdin);
            printf("새 저자: "); fgets(a, BUF_SIZE, stdin);
            printf("새 평점: "); fgets(r, BUF_SIZE, stdin);
            num[strcspn(num,"\n")] = t[strcspn(t,"\n")] = a[strcspn(a,"\n")] = r[strcspn(r,"\n")] = 0;

            snprintf(message, sizeof(message), "4/%s\t%s\t%s\t%s", num, t, a, r);
            send(sock, message, strlen(message), 0);
            while ((n = recv(sock, buf, sizeof(buf)-1, 0)) > 0) {
                buf[n] = '\0';
                if (strstr(buf, "[END]\n")) break;
                printf("%s", buf);
            }
            printf("도서 수정 완료\n");
        }else if(strcmp(keyword, "5") == 0){
    printf("평점 정렬 기능\n");
    snprintf(message, sizeof(message), "5/");
    send(sock, message, strlen(message), 0);

    while ((n = recv(sock, buf, sizeof(buf)-1, 0)) > 0) {
        buf[n] = '\0';
        if (strstr(buf, "[END]\n")) break;
        printf("%s", buf);  // 🔸 실제 내용을 출력
    }
    printf("평점 정렬 완료\n");
        }else if(strcmp(keyword, "6") == 0){
    printf("사용자 추가\n");
    printf("아이디: ");
    fgets(id, sizeof(id), stdin);
    id[strcspn(id, "\n")] = '\0';
    printf("비밀번호: ");
    fgets(pass, sizeof(pass), stdin);
    pass[strcspn(pass, "\n")] = '\0';
    sprintf(message, "6/%s//%s", id, pass);  
    send(sock, message, strlen(message), 0);
    
    while ((n = recv(sock, buf, sizeof(buf)-1, 0)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
        if (strstr(buf, "[END]\n") || strstr(buf, "[END]")) break; 
    }
        }else if(strcmp(keyword, "7") == 0){
            printf("사용자 삭제\n");
            printf("삭제할 아이디: ");
            fgets(id, sizeof(id), stdin);
            id[strcspn(id, "\n")] = '\0';
            sprintf(message, "7/%s", id);
            send(sock, message, strlen(message), 0);
             while ((n = recv(sock, buf, sizeof(buf)-1, 0)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
        if (strstr(buf, "[END]\n") || strstr(buf, "[END]")) break; 
    }   
        }else if(strcmp(keyword, "8") == 0){
            printf("종료\n");
            sprintf(message, "8");
            send(sock, message, strlen(message), 0);
            break;
        }
        
    }
    // WaitForSingleObject(sendThread,INFINITE);
	// WaitForSingleObject(recvThread,INFINITE);
 

    closesocket(sock);
    WSACleanup();
    return 0;
     }



unsigned WINAPI SendMsg(void* arg){
	SOCKET sock=*((SOCKET*)arg);
	char msg[BUF_SIZE];
	while(1){
		fgets(msg,BUF_SIZE,stdin);
		if(!strcmp(msg,"8\n")){
			send(sock,"8",1,0);
		}
		sprintf(msg,"%s %s",name,msg);
		send(sock,msg,strlen(msg),0);
	}
	return 0;
}

unsigned WINAPI RecvMsg(void* arg){
	SOCKET sock=*((SOCKET*)arg);
	char msg[BUF_SIZE];
	int strLen;
	while(1){
		strLen=recv(sock,msg,BUF_SIZE-1,0);
		if(strLen==-1)
			return -1;
		msg[strLen]=0;
		if(!strcmp(msg,"8")){
			printf("종료\n");
			closesocket(sock);
			exit(0);
		}
		fputs(msg,stdout);
	}
	return 0;
}

void ErrorHandling(const char* msg){
	fputs(msg,stderr);
	fputc('\n',stderr);
	exit(1);
}