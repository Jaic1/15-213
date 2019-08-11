#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

static char dd[100];

int main(int argc, char *argv[])
{
    if (argc != 1 || argv[1][0] != '0' || argv[1][1] != 'x' || strlen(argv[1] + 2) != 8)
    {
        fprintf(stderr, "Error: not hex number!\n");
        exit(0);
    }

    uint32_t x;
    sscanf(argv[1] + 2, "%x", &x);
    x = htonl(x);

    if (inet_ntop(AF_INET, &x, dd, 100) == NULL)
    {
        fprintf(stderr, "Error: translation error!\n");
        exit(0);
    }

    fprintf(stdout, "%s\n", dd);
    return 0;
}