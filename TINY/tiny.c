/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"

/* define service type */
#define SERVICE_STATIC 0
#define SERVICE_DYNAMIC 1
#define SERVICE_MP4 2

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

/* signal handler function*/
void sigchld_handler(int sig);

void doit(int fd);
void read_requesthdrs(rio_t *rp, char *range, char *content_type, int *content_length_ptr);
int parse_uri(char *uri, char *filetype, char *filename, char *cgiargs);
void serve_static(int fd, char *filetype, char *filename, int is_head);
void get_filetype(char *filename, char *filetype);
void serve_mp4(int fd, char *filename, char *range, int is_head);
void serve_dynamic(int fd, char *filename, char *cgiargs, int is_head);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);

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

    /* register signal handler */
    Signal(SIGCHLD, sigchld_handler);

    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);
        Close(connfd);
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd)
{
    int servtype, is_head;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filetype[MAXLINE], filename[MAXLINE], cgiargs[MAXLINE];
    char range[MAXLINE], content_type[MAXLINE];
    int content_length;
    rio_t rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    read_requesthdrs(&rio, range, content_type, &content_length);

    /* determine request method */
    is_head = 0;
    if (!strcasecmp(method, "GET"))
        servtype = parse_uri(uri, filetype, filename, cgiargs);
    else if (!strcasecmp(method, "HEAD"))
    {
        is_head = 1;
        servtype = parse_uri(uri, filetype, filename, cgiargs);
    }
    else if (!strcasecmp(method, "POST"))
    {
        if (content_type[0] == '\0' || !strstr(content_type, "application/x-www-form-urlencoded"))
        {
            clienterror(fd, content_type, "415", "Unsupported Media Type",
                        "Tiny does not support this content-type(or not specified) for post");
            return;
        }
        strcpy(filename, ".");
        strcat(filename, uri);
        /* Rio_readlineb(&rio, cgiargs, MAXLINE);
         * can not be used, because Chrome, for example, 
         * will only send n1=15&n2=23 but without \r\n or EOF
         * so we should pass in the Content-Length param
         */
        Rio_readlineb(&rio, cgiargs, MIN(content_length+1, MAXLINE));
        servtype = SERVICE_DYNAMIC;
    }
    else
    {
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }

    /* serve content */
    switch (servtype)
    {
    case SERVICE_STATIC:
        serve_static(fd, filetype, filename, is_head);
        break;
    case SERVICE_DYNAMIC:
        serve_dynamic(fd, filename, cgiargs, is_head);
        break;
    case SERVICE_MP4:
        serve_mp4(fd, filename, range, is_head);
        break;
    default:
        clienterror(fd, filename, "400", "Bad Request",
                    "Tiny couldn't understand the request due to invalid syntax");
    }
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp, char *range, char *content_type, int *content_length_ptr)
{
    char buf[MAXLINE];
    range[0] = '\0';
    content_type[0] = '\0';
    *content_length_ptr = 0;

    do
    {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
        if (strstr(buf, "Range"))
            strcpy(range, buf);
        else if (strstr(buf, "Content-Type"))
            strcpy(content_type, buf);
        else if(strstr(buf, "Content-Length"))
            sscanf(buf, "Content-Length: %d", content_length_ptr);
    } while (strcmp(buf, "\r\n"));

    return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filetype, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, "cgi-bin"))
    { /* Static content (including mp4) */
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if (uri[strlen(uri) - 1] == '/')
            strcat(filename, "home.html");
        get_filetype(filename, filetype);

        return strstr(filename, "mp4") ? SERVICE_MP4 : SERVICE_STATIC;
    }
    else
    { /* Dynamic content */
        ptr = index(uri, '?');
        if (ptr)
        {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        return SERVICE_DYNAMIC;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filetype, char *filename, int is_head)
{
    int srcfd, filesize;
    struct stat sbuf;
    char *srcp, buf[MAXBUF];

    /* check file stat */
    if (stat(filename, &sbuf) < 0)
    {
        clienterror(fd, filename, "404", "Not found",
                    "Tiny couldn't find this file");
        return;
    }
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
        clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't read the file");
        return;
    }
    filesize = sbuf.st_size;

    /* Send response headers to client */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    /* Send response body to client */
    if (is_head)
        return;
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

/*
 * get_filetype - derive file type from file name
 */
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
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");
    else
        strcpy(filetype, "text/plain");
}
/* $end serve_static */

/*
 * serve_mp4 - serve a portion of mp4 file (max to 2G)
 *           - to let client stream it
 * P.S. mp4 can be streamed in Chrome,
 *      but cannot be downloaded.
 */
/* $begin serve_mp4 */
void serve_mp4(int fd, char *filename, char *range, int is_head)
{
    int srcfd, filesize;
    int start, end, length;
    int page_start;
    int page = sysconf(_SC_PAGE_SIZE);
    struct stat sbuf;
    char *srcp, buf[MAXLINE];

    /* check file stat */
    if (stat(filename, &sbuf) < 0)
    {
        clienterror(fd, filename, "404", "Not found",
                    "Tiny couldn't find this file");
        return;
    }
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
        clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't read the file");
        return;
    }
    filesize = sbuf.st_size;

    if (range[0] != '\0')
    {
        if (sscanf(range, "Range: bytes=%d-%d", &start, &end) != 2)
            end = MIN(filesize - 1, start + page * 5 - 1);
        else
            end = MIN(filesize - 1, end);
        page_start = start / page;
        length = end + 1 - page_start * page;
    }
    else
    {
        sprintf(buf, "HTTP/1.1 200 OK\r\n");
        sprintf(buf, "%sAccept-Ranges: bytes\r\n", buf);
        sprintf(buf, "%sContent-Type: video/mp4\r\n", buf);
        sprintf(buf, "%sContent-Length: %d\r\n", buf, filesize);
        printf("Response headers:\n");
        printf("%s", buf);
        Rio_writen(fd, buf, strlen(buf));
        return;
    }

    /* Send response headers to client */
    sprintf(buf, "HTTP/1.1 206 Partial Content\r\n");
    sprintf(buf, "%sContent-Range: bytes %d-%d/%d\r\n",
            buf, start, end, filesize);
    sprintf(buf, "%sContent-Length: %d\r\n\r\n", buf, end + 1 - start);
    printf("Response headers:\n");
    printf("%s", buf);
    Rio_writen(fd, buf, strlen(buf));

    /* Send response body to client */
    if (is_head)
        return;
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, length, PROT_READ, MAP_PRIVATE, srcfd, page_start * page);
    Close(srcfd);
    Rio_writen(fd, srcp + (start - page_start * page), end + 1 - start);
    Munmap(srcp, length);
}
/* $end serve_mp4 */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs, int is_head)
{
    struct stat sbuf;
    char buf[MAXLINE], *emptylist[] = {NULL};

    /* check file stat */
    if (stat(filename, &sbuf) < 0)
    {
        clienterror(fd, filename, "404", "Not found",
                    "Tiny couldn't find this file");
        return;
    }
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
        clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't run the CGI program");
        return;
    }

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    printf("Response headers:\n");
    printf("%s(...Left by cgi program)\n\n", buf);
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0)
    { /* Child */
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1);
        if (is_head)
            setenv("IS_HEAD", "HEAD", 1);
        Dup2(fd, STDOUT_FILENO);              /* Redirect stdout to client */
        Execve(filename, emptylist, environ); /* Run CGI program */
    }
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor="
                  "ffffff"
                  ">\r\n",
            body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

/*
 * sigchld_handler - reap child process
 */
void sigchld_handler(int sig)
{
    int olderrno = errno;

    while (waitpid(-1, NULL, 0) > 0)
        Sio_puts("sigchld_handler reaped child\n");
    if (errno != ECHILD)
        Sio_error("waitpid error");
    errno = olderrno;
}