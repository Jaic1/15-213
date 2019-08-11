#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        fprintf(stderr, "Error: not hex number!\n");
        exit(0);
    }

    uint32_t x;
    if(inet_pton(AF_INET, argv[1], &x) != 1)
    {
        fprintf(stderr, "Error: translation error!\n");
        exit(0);
    }    

    x = ntohl(x);
    fprintf(stdout, "0x%x\n", x);
    return 0;
}