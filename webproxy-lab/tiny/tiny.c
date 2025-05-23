/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, int is_head);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}

void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  int is_head = 0; // HEAD 요청 여부

  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);

  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")){
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  
  if (!strcasecmp(method, "HEAD")) {
    is_head = 1;  // HEAD 요청임을 표시
  }
  
  read_requesthdrs(&rio);
  is_static = parse_uri(uri, filename, cgiargs);

  if (stat(filename, &sbuf) < 0){
    clienterror(fd, filename, "404", " Not found", "Tiny couldn't find the file");
    return;
  }

  if (is_static){
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", " Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, is_head);
  }
  else{
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", " Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    if (!is_head)  // HEAD 요청일 때는 CGI 실행하지 않음
      serve_dynamic(fd, filename, cgiargs);
  }
}


void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];


  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) {
    strcpy(cgiargs, "");                 
    strcpy(filename, ".");                    
    strcat(filename, uri);                    
    if (uri[strlen(uri)-1] == '/')            
      strcat(filename, "home.html");
    return 1;                                
  }
  else { 
    ptr = index(uri, '?');                   
    if (ptr) {
      strcpy(cgiargs, ptr+1);                
      *ptr = '\0';                          
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);                 
    return 0;                             
  }
}

//malloc으로
// void serve_static(int fd, char *filename, int filesize) {
//   int srcfd;
//   char *srcp, filetype[MAXLINE], buf[MAXBUF];

//   // 1. 파일 타입 결정 (기존 코드 유지)
//   get_filetype(filename, filetype);

//   // 2. HTTP 응답 헤더 생성 및 전송 (기존 코드 유지)
//   sprintf(buf, "HTTP/1.0 200 OK\r\n");
//   sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
//   sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
//   sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
//   Rio_writen(fd, buf, strlen(buf));
//   printf("Response headers:\n%s", buf);

//   // 3. 파일 열기
//   srcfd = Open(filename, O_RDONLY, 0);

//   // 4. 메모리 할당 (mmap 대신 malloc 사용)
//   srcp = (char *)malloc(filesize);
//   if (srcp == NULL) {
//       fprintf(stderr, "Error: malloc failed\n");
//       Close(srcfd);
//       exit(1);
//   }

//   // 5. 파일 내용 읽기 (rio_readn 사용)
//   Rio_readn(srcfd, srcp, filesize);
//   Close(srcfd); // 파일 읽은 후 즉시 닫음

//   // 6. 클라이언트에 데이터 전송 (rio_writen 사용)
//   Rio_writen(fd, srcp, filesize);

//   // 7. 할당된 메모리 해제
//   free(srcp);
// }


void serve_static(int fd, char *filename, int filesize, int is_head)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  get_filetype(filename, filetype);

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));  
  printf("Response headers:\n");
  printf("%s", buf);

  if (is_head)  // HEAD 요청이면 본문 생략
    return;

  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);

  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize);
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".mpg") || strstr(filename, ".mpeg"))     //문제 11.7
    strcpy(filetype, "video/mpeg");
  else
    strcpy(filetype, "text/plain");  
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { 
    setenv("QUERY_STRING", cgiargs, 1); 
    Dup2(fd, STDOUT_FILENO);             
    Execve(filename, emptylist, environ); 
  }
  Wait(NULL);
}
