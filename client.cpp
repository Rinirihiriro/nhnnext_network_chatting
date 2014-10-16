#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define WRITE_DATA "!dlroW olleH"

int main(const int argc, const char * const * cont argv)
{
    if (argc != 4)
    {
        printf("USAGE : %s <IP> <PORT> <NICKNAME>", argv[0]);
        return -1;
    }

    int ret = -1;
    int clientSock;
    struct sockaddr_in serverAddr;

    if ((clientSock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        goto leave;
    }

    unsigned int ip = inet_addr(argv[1]);
    unsigned short port = atoi(argv[2]);

    if (ip == INADDR_NONE || ip == INADDR_ANY)
    {
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr();
    serverAddr.sin_port = htons();

    if ((ret = connect(clientSock,(struct sockaddr*)&serverAddr,
                       sizeof(serverAddr)))) {
        perror("connect");
        goto error;
    }

    if ((ret = send(clientSock, WRITE_DATA, sizeof(WRITE_DATA), 0)) <= 0) {
        perror("send");
        ret = -1;
    } else
        printf("Wrote '%s' (%d Bytes)\n", WRITE_DATA, ret);

error:
    close(clientSock);
leave:
    return ret;
}

