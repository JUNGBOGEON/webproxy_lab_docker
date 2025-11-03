/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd); // 클라이언트 연결에 성공했을 때 실행 함수
void read_requesthdrs(rio_t *rp); // HTTP 요청 메시지 헤더(User-Agent, Accept) 읽기
int parse_uri(char *uri, char *filename, char *cgiargs); // URL 분석 (동적인지 정적인지)
void serve_static(int fd, char *filename, int filesize); // 정적이라 판단했을때 호출
void get_filetype(char *filename, char *filetype); // 파일 확장자 확인
void serve_dynamic(int fd, char *filename, char *cgiargs); // 동적이라 판단했을때 호출
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, // 요청 처리 중 오류가 발생 했을때 호출
                 char *longmsg);

int main(int argc, char **argv) // [argc] 실행될때 전딜된 인자의 개수 | ./tiny 8000 이면 argc 는 2개 (0, 1)
                                // [argv] 인자 문자열들의 배열 | argv[0] = tiny, argv[1] = 8000
{
  int listenfd, connfd; // 듣기 소켓, 연결 소켓 선언
  char hostname[MAXLINE], port[MAXLINE]; // 연결된 클라이언트의 호스트 이름과 포트 번호 저장
  socklen_t clientlen; // 클라 주소 정보 구조체
  struct sockaddr_storage clientaddr; // 연결된 클라의 소켓 주소 정보를 저장하기 위한 범용 구조체 | IPv4, IPv6 주소를 모두 담을 정도의 크기여야 하

  /* Check command line args */
  if (argc != 2) // 실행될때 인자가 2개가 아닐 경우, 사용 방법이 잘못 되었을 때
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 사용자가 명령줄 인자로 전달한 포트 번호를 Open_listenfd 함수에 넘김
  while (1) // 계속 요청을 받을 수 있게 무한 루프
  {
    clientlen = sizeof(clientaddr); // accept 함수를 호출하기 전 clientaddr 구조체가 차지하는 크기 저장
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 요청 대기하기
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 사람이 읽을 수 있는 형태로 호스트 이름과 포트로 변환
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // 요청 수신 및 응답 전송을 처리하기 위해 doit 함수 호출
    Close(connfd); // 모든 처리 완료 -> 연결 종료
  }
}

void doit(int fd)
{
  int is_static; // 요청이 정적(1) 인지 동적(0) 인지 저장할 플래그 변수
  struct stat sbuf; // 함수를 통해 얻어온 파일의 정보를 저장할 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  // buf 는 소켓에서 데이터를 읽어올 때 사용할 임시 버퍼
  // method, uri, version 은 HTTP 요청 라인에서 파싱한 각 부분을 저장할 문자열 버퍼
  char filename[MAXLINE], cgiargs[MAXLINE]; // uri 를 분석한 후 실제 파일 경로와 CGI 인자를 저장할 버퍼
  rio_t rio; // 안정적인 입출력(Robust I/O)을 위한 RIO 구조체

  // 요청 라인 읽기 및 파싱
  Rio_readinitb(&rio, fd); // RIO 읽기 구조체(rio)를 초기화하고, fd(연결 소켓)와 연결한다. 이제 rio 를 통해 fd 에서 데이터를 읽을 수 있다
  Rio_readlineb(&rio, buf, MAXLINE); // 클라이언트가 보낸 HTTP 요청의 첫 번째 줄을 \n 문자를 만날 때까지 읽어서 buf 에 저장
  printf("Request headers: \n"); // 방금 읽은 요청 라인을 출력
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); // buf 에 저장된 문자열을 공백을 기준으로 세 부분으로 나누어, 각각 method, uri, version 변수에 저장
  if (strcasecmp(method, "GET")) { // method가 "GET" 인지 대소문자 구분 없이 비교
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    // 메서드가 GET 이 아니라면 (예: POST), clienterror 함수를 호출하여 클라에게 501 (구현안됨) 오류 응답을 보내고 doit 함수 종료
    return;
  }
  read_requesthdrs(&rio);
  // 메서드가 GET 인 경우 요청 라인 다음에 오는 HTTP 헤더들을 읽는다.

  is_static = parse_uri(uri, filename, cgiargs); // parse_uri 함수가 uri(예: "/index.html") 를 분석한다
  if (stat(filename, &sbuf) < 0) { // stat 함수를 사용해 filename 에 해당하는 파일이 디스크에 실제로 존재하는지 확인히고, 파일이 있다면 그 정보를 sbuf 구조체에 채움
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file"); // 파일이 존재하지 않을때 요청 처리
    return;
  }

  // 정적/동적 콘텐츠 분기 처리
  if (is_static) {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read this file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }
  else {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXLINE];

  // 'body' 버퍼에 HTML 문자열을 순차적으로 조립합니다.
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body); // 기존 body 내용에 덧붙임
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg); // 예: 404: Not Found
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body); // 하단 푸터

  // (1) HTTP 상태 라인 전송
  sprintf(buf, "HTTP/1.0 %s %s \r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  // (2) HTTP 헤더 전송
  sprintf(buf, "Content-Type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body)); // 헤더의 끝(\r\n\r\n)
  Rio_writen(fd, buf, strlen(buf));

  // (3) HTTP 본문 (앞서 만든 HTML) 전송
  Rio_writen(fd, body, strlen(body));
}