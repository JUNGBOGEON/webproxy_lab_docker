/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 * GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 * - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

/* HTTP 트랜잭션(요청 처리 및 응답)을 수행하는 함수 */
void doit(int fd);

/* HTTP 요청 헤더를 읽고 무시하는 함수 */
void read_requesthdrs(rio_t *rp);

/* URI를 파싱하여 파일 이름과 CGI 인자를 추출하고, 정적/동적 컨텐츠를 구분하는 함수 */
int parse_uri(char *uri, char *filename, char *cgiargs);

/* 정적 컨텐츠를 클라이언트에게 제공하는 함수 */
void serve_static(int fd, char *filename, int filesize, char *method);

/* 파일 확장자를 기반으로 MIME 타입을 결정하는 함수 */
void get_filetype(char *filename, char *filetype);

/* 동적 컨텐츠(CGI 프로그램)를 실행하여 클라이언트에게 제공하는 함수 */
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);

/* 클라이언트에게 오류를 보고하는 함수 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* -------------------
 * main 함수
 * ------------------- */
int main(int argc, char **argv) // argc: 커맨드 라인 인자의 개수, argv: 인자 문자열 배열
{
  int listenfd, connfd;                   // 듣기 소켓 식별자, 연결 소켓 식별자
  char hostname[MAXLINE], port[MAXLINE]; // 연결된 클라이언트의 호스트 이름과 포트 번호
  socklen_t clientlen;                  // 클라이언트 주소 구조체의 크기
  struct sockaddr_storage clientaddr;   // 클라이언트의 소켓 주소 정보를 저장할 구조체 (IPv4/IPv6 모두 호환)

  /* Check command line args (명령줄 인자 확인) */
  if (argc != 2) // 프로그램 실행 시 포트 번호를 인자로 받지 않은 경우
  {
    // 사용법을 stderr(표준 오류)에 출력
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1); // 프로그램 비정상 종료
  }

  /* 지정된 포트 번호로 듣기 소켓(listen socket)을 연다 */
  listenfd = Open_listenfd(argv[1]);

  /* 무한 루프: 서버가 계속해서 연결 요청을 기다리도록 함 */
  while (1)
  {
    clientlen = sizeof(clientaddr); // accept 함수 호출 전, 주소 구조체의 크기를 설정

    /* 클라이언트의 연결 요청을 기다리고, 연결되면 connfd(연결 소켓)를 반환 */
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // (SA *)는 (struct sockaddr *)로의 형변환

    /* 클라이언트의 주소 정보를 사람이 읽을 수 있는 호스트 이름과 포트 번호로 변환 */
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    /* HTTP 트랜잭션 처리 (요청을 받고 응답을 보냄) */
    doit(connfd);

    /* 트랜잭션 처리가 완료되면 클라이언트와의 연결을 닫음 */
    Close(connfd);
  }
}

/* -------------------
 * doit: HTTP 트랜잭션 처리
 * ------------------- */
void doit(int fd) // fd: 연결된 클라이언트 소켓의 파일 식별자
{
  int is_static; // 정적 컨텐츠인지(1) 동적 컨텐츠인지(0) 구분하는 플래그
  struct stat sbuf; // 파일의 메타데이터(크기, 권한 등)를 저장할 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE]; // 파싱된 파일 이름과 CGI 인자를 저장할 버퍼
  rio_t rio; // Robust I/O 버퍼 구조체

  /* Read request line and headers (요청 라인과 헤더 읽기) */
  Rio_readinitb(&rio, fd);             // rio 버퍼를 초기화하고 fd와 연결
  Rio_readlineb(&rio, buf, MAXLINE);   // rio 버퍼에서 한 줄(요청 라인)을 읽어 buf에 저장
  printf("Request headers:\n");
  printf("%s", buf); // 서버 콘솔에 요청 라인 출력 (예: "GET / HTTP/1.1")

  /* 요청 라인(buf)에서 메소드, URI, HTTP 버전을 파싱 */
  sscanf(buf, "%s %s %s", method, uri, version);

  do {
    if (rio_readlineb(&rio, buf, MAXLINE) <= 0) {
        return; 
    }
    printf("%s", buf); 
  } while(strcmp(buf, "\r\n")); // 빈 줄(\r\n)을 만날 때까지 반복

  /* Tiny 서버는 GET과 HEAD 메소드만 지원 */
  // strcasecmp: 대소문자 구분 없이 문자열 비교
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    // 지원하지 않는 메소드일 경우 501 Not Implemented 에러 전송
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return; // 함수 종료
  }

  /* 나머지 요청 헤더들을 읽고 무시 */
  // read_requesthdrs(&rio);

  /* Parse URI from GET request (GET 요청에서 URI 파싱) */
  // URI를 파싱하여 filename과 cgiargs를 채우고, 정적/동적 여부(is_static)를 리턴
  is_static = parse_uri(uri, filename, cgiargs);
  printf("uri: %s, filename: %s, cgiargs: %s \n", uri, filename, cgiargs);

  /* stat 함수: 파일 이름(filename)에 해당하는 파일의 메타데이터를 sbuf에 저장 */
  /* 파일이 존재하지 않으면 -1 리턴 */
  if (stat(filename, &sbuf) < 0)
  {
    // 파일이 없으면 404 Not Found 에러 전송
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  /* Serve static content (정적 컨텐츠 제공) */
  if (is_static)
  {
    // S_ISREG: 일반 파일인지 확인
    // S_IRUSR & sbuf.st_mode: 소유자 읽기 권한(R)이 있는지 확인
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      // 파일이 아니거나 읽기 권한이 없으면 403 Forbidden 에러 전송
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 정적 파일 제공
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else /* Serve dynamic content (동적 컨텐츠 제공) */
  {
    // S_ISREG: 일반 파일인지 확인
    // S_IXUSR & sbuf.st_mode: 소유자 실행 권한(X)이 있는지 확인
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      // 파일이 아니거나 실행 권한이 없으면 403 Forbidden 에러 전송
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 동적 컨텐츠(CGI) 실행
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

/* -------------------
 * clienterror: 오류 응답 전송
 * ------------------- */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF]; // 응답 헤더용 buf, 응답 본문(HTML)용 body

  /* [FIX] sprintf 버퍼 겹침(aliasing) 오류를 피하기 위해
   * buf + strlen(buf) 대신 임시 버퍼를 사용하거나,
   * 여기서는 body + strlen(body)를 사용해 버퍼의 끝에 안전하게 이어쓰기 */

  /* Build the HTTP response body (HTML 본문 생성) */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body + strlen(body), "<body bgcolor=\"ffffff\">\r\n");
  sprintf(body + strlen(body), "%s: %s\r\n", errnum, shortmsg);
  sprintf(body + strlen(body), "<p>%s: %s\r\n", longmsg, cause);
  sprintf(body + strlen(body), "<hr><em>The Tiny Web Server</em>\r\n");

  /* Print the HTTP response (HTTP 응답 전송) */
  // 1. 상태 라인 (예: "HTTP/1.0 404 Not Found\r\n")
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  // 2. 헤더
  sprintf(buf, "Content-Type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 3. 본문 길이 헤더 (헤더의 끝을 알리는 \r\n\r\n 포함)
  sprintf(buf, "Content-Length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  // 4. 본문 (HTML)
  Rio_writen(fd, body, strlen(body));
}

/* -------------------
 * read_requesthdrs: 요청 헤더 읽기
 * ------------------- */
/* Tiny는 요청 헤더 정보를 사용하지 않으므로, 읽고 무시함 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE); // 첫 번째 헤더 라인 읽기
  
  /* 헤더의 끝은 빈 줄("\r\n")이므로,
   * 읽어온 라인이 "\r\n"이 아닐 때까지 루프를 계속 돔 */
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE); // 다음 헤더 라인 읽기
    printf("%s", buf); // 헤더 라인 출력 (디버깅용)
  }
  return;
}

/* -------------------
 * parse_uri: URI 파싱
 * ------------------- */
/* URI를 파싱하여 filename(파일 경로)과 cgiargs(CGI 인자)를 추출 */
/* 리턴 값: 1 (정적 컨텐츠), 0 (동적 컨텐츠) */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  /* strstr: 문자열(uri)에 "cgi-bin"이 포함되어 있는지 확인 */
  if (!strstr(uri, "cgi-bin"))
  { /* Static content (정적 컨텐츠) */
    strcpy(cgiargs, ""); // 정적 컨텐츠는 CGI 인자가 없음
    strcpy(filename, "."); // 현재 디렉토리를 의미하는 "."
    strcat(filename, uri); // filename에 URI 경로를 붙임 (예: "./index.html")

    // 만약 URI가 '/'로 끝난다면 (예: http://host:port/)
    if (uri[strlen(uri) - 1] == '/')
    {
      strcat(filename, "home.html"); // 기본 파일인 "home.html"을 붙여줌
    }
    return 1; // 정적 컨텐츠 플래그 리턴
  }
  else
  { /* Dynamic content (동적 컨텐츠) */
    // URI 예시: /cgi-bin/adder?first=12&second=34
    
    // '?' 문자의 위치를 찾음
    ptr = index(uri, '?');

    if (ptr) // '?'가 있다면 (CGI 인자가 있다면)
    {
      strcpy(cgiargs, ptr + 1); // '?' 다음 문자열
      *ptr = '\0'; // '?' 위치를 널 문자로 바꿔 URI 문자열을 분리
    }
    else // '?'가 없다면 (CGI 인자가 없다면)
    {
      strcpy(cgiargs, "");
    }

    strcpy(filename, "."); // 현재 디렉토리
    strcat(filename, uri); // CGI 프로그램 경로 (예: "./cgi-bin/adder")
    
    return 0; // 동적 컨텐츠 플래그 리턴
  }
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".heic"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4")) // (CSAPP 11.7 숙제)
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

/* -------------------
 * serve_static: 정적 컨텐츠 제공
 * ------------------- */
void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd; // 원본 파일의 식별자
  char *srcp; // 메모리에 매핑된 파일의 시작 주소 포인터
  char filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client (클라이언트에게 응답 헤더 전송) */
  get_filetype(filename, filetype); // 파일 타입 결정

  /* [FIX] sprintf 버퍼 겹침(aliasing) 오류 수정 */
  /* buf + strlen(buf)를 사용해 버퍼의 끝(널 문자 위치)부터 이어쓰기 */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
  sprintf(buf + strlen(buf), "Connection: close\r\n");
  sprintf(buf + strlen(buf), "Content-Length: %d\r\n", filesize);
  sprintf(buf + strlen(buf), "Content-Type: %s\r\n\r\n", filetype);

  // 완성된 헤더를 클라이언트에게 한 번에 전송
  Rio_writen(fd, buf, strlen(buf));

  // 서버 콘솔에 응답 헤더 출력
  printf("Response headers:\n");
  printf("%s", buf);

  // if (!strcasecmp(method, "HEAD"))
  // {
  //   return;
  // }

  /* Send response body to client (클라이언트에게 응답 본문 전송) */
  srcfd = Open(filename, O_RDONLY, 0); // 파일 열기 (읽기 전용)

  /* [IMPROVE] Malloc/Rio_readn 대신 mmap 사용 */
  // mmap: 파일을 프로세스의 가상 메모리 공간에 직접 매핑 (데이터 복사 최소화)
  // PROT_READ: 매핑된 페이지는 읽기만 가능
  // MAP_PRIVATE: 다른 프로세스와 매핑 공유 안 함 (쓰기 시 copy-on-write)
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  
  Close(srcfd); // mmap을 호출한 후에는 파일 식별자를 닫아도 메모리 매핑은 유지됨

  /* 매핑된 메모리(srcp)의 내용을 클라이언트에게 전송 */
  Rio_writen(fd, srcp, filesize);
  
  /* Munmap: 메모리 매핑 해제 */
  Munmap(srcp, filesize);
}

/* -------------------
 * serve_dynamic: 동적 컨텐츠 제공
 * ------------------- */
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first part of HTTP response (HTTP 응답의 첫 부분 전송) */
  // 클라이언트에게 성공(200 OK)을 먼저 알림
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server 1\r\n");
  Rio_writen(fd, buf, strlen(buf));
  // (CGI 프로그램이 나머지 헤더와 본문을 생성할 것임)

  /* 자식 프로세스 생성 (Fork) */
  if (Fork() == 0)
  { /* Child (자식 프로세스) */
    /* CGI 프로그램이 사용할 환경 변수 설정 */
    // QUERY_STRING: CGI 인자 (예: "first=12&second=34")
    setenv("QUERY_STRING", cgiargs, 1);
    // REQUEST_METHOD: HTTP 메소드 (예: "GET")
    setenv("REQUEST_METHOD", method, 1);

    /* 자식 프로세스의 표준 출력(STDOUT_FILENO)을
     * 클라이언트와 연결된 소켓 식별자(fd)로 리다이렉션(재지정) */
    Dup2(fd, STDOUT_FILENO); // 이제 자식 프로세스에서 printf/write 등으로 출력하는 모든 것은 클라이언트에게 바로 전송됨

    /* CGI 프로그램(filename)을 현재 프로세스(자식)의 메모리에 로드하고 실행 */
    // emptylist: 인자 목록 (여기서는 없음)
    // environ: 환경 변수 목록
    Execve(filename, emptylist, environ);
  }

  /* Parent (부모 프로세스) */
  Wait(NULL); // 자식 프로세스가 종료될 때까지 기다림
}