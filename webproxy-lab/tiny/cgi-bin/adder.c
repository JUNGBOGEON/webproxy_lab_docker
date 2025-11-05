
/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "/workspaces/webproxy_lab_docker/webproxy-lab/tiny/csapp.h"

int main(void) {
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1 = 0, n2 = 0;
   
    // QUERY_STRING 환경변수에 저장된, HTTP Request 인자로 받은 값을 getenv 함수로 가져온다
    if((buf = getenv("QUERY_STRING")) != NULL) {

    // 인자는 '&'로 구분되어 있으므로 '&'를 기준으로 나눈다. p는 &를 가리킨다
        p = strchr(buf, '&');
    // &를 널 문자로 바꿔, 각 문자열(인자)들을 널 문자 기준으로 구분한다.
    // 마치 배열처럼 사용할 수 있게 된다.
        *p = '\0';
    // 첫 번째 인자를 arg1에 저장
        strcpy(arg1, buf);
    // 두 번째 인자를 arg2에 저장
        strcpy(arg2, p+1);

    // arg1, arg2 에 있는 값을 정수로 변환하여 저장한다
        n1 = atoi(arg1);
        n2 = atoi(arg2);
    }

    // Response Body 만들기
  // sprintf는 출력하는 대신 문자열로써 저장한다 ( content에 저장 )
  // sprintf 호출 시 마다 content에 저장된 문자열에 이어 붙이게 된다.
    sprintf(content, "QUERY_STRING=%s", buf);
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
    sprintf(content, "%sThanks for visiting!\r\n", content);

    // Response Header/Body 출력
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);

  // 표준 출력은 기본적으로 버퍼링된다.
  // printf 함수에서 표준출력된 데이터들을 클라이언트에게 전송한다.
    fflush(stdout);

  exit(0);
}
/* $end adder */
