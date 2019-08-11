/* $begin hostinfo */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MAXLINE 8192 /* Max text line length */

int main(int argc, char **argv)
{
    struct addrinfo *p, *listp;
    struct addrinfo hints;
    char buf[MAXLINE];
    int rc, flags;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
        exit(0);
    }

    /* Get a list of addrinfo records */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; /* IPv4 only */              //line:netp:hostinfo:family
    hints.ai_socktype = SOCK_STREAM; /* Connections only */ //line:netp:hostinfo:socktype
    if ((rc = getaddrinfo(argv[1], NULL, &hints, &listp)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
        exit(1);
    }

    /* Walk the list and display each IP address */
    flags = NI_NUMERICHOST; /* Display address string instead of domain name */
    for (p = listp; p; p = p->ai_next)
    {
        getnameinfo(p->ai_addr, p->ai_addrlen, buf, MAXLINE, NULL, 0, flags);

        // practice 11.4
        // uint32_t x;
        // struct sockaddr_in *sa_in;
        // sa_in = (struct sockaddr_in *)(p->ai_addr);
        // x = sa_in->sin_addr.s_addr;
        // inet_ntop(AF_INET, &x, buf, MAXLINE);

        printf("%s\n", buf);
    }

    /* Clean up */
    freeaddrinfo(listp);

    exit(0);
}
/* $end hostinfo */
