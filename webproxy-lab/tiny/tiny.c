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
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers: \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

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