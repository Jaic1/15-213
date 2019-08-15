/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

void sigpipe_handler(int sig);

int main(void)
{
    char *buf;
    char content[MAXLINE];
    int n1 = 0, n2 = 0, is_head;

    /* register signal handler */
    Signal(SIGPIPE, sigpipe_handler);

    /* Extract the two arguments */
    if ((buf = getenv("QUERY_STRING")) != NULL)
        sscanf(buf, "n1=%d&n2=%d", &n1, &n2);

    /* Check if the method is HEAD */
    is_head = 0;
    if ((buf = getenv("IS_HEAD")) != NULL && !strcmp(buf, "HEAD"))
        is_head = 1;

    /* Make the response body */
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
            content, n1, n2, n1 + n2);
    sprintf(content, "%sThanks for visiting!\r\n", content);

    /* Generate the HTTP response */
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    if (!is_head)
        printf("%s", content);
    fflush(stdout);

    exit(0);
}
/* $end adder */

/*
 * sigpipe_handler - forward SIGPIPE to proc group
 */
void sigpipe_handler(int sig)
{
    Sio_puts("Connection closed by foreign host.\n");
    exit(0);
}