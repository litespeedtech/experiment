#include "do_test.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int do_connect(const char *address, int port)
{
    struct addrinfo  hints;
    struct addrinfo *result, *rp;
    int err;
    int sock = -1;
    char s_port[256];
    char last_error[8192] = { 0 };
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family     = AF_UNSPEC; // v4 or v6
    hints.ai_socktype   = SOCK_STREAM;
    
    snprintf(s_port, sizeof(s_port), "%d", port);
    err = getaddrinfo(address, s_port, &hints, &result);
    
    if (err)
    {
        printf("Error converting %s/%s to an address: %s\n", address, s_port,
               gai_strerror(err));
        return -1;
    }
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1)
        {
            snprintf(last_error, sizeof(last_error), "Socket for %s/%s "
                     "returned: %s", address, s_port, strerror(errno));
            continue;
        }
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) != -1)
            break;
        snprintf(last_error, sizeof(last_error), "connect for %s/%s "
                 "returned: %s", address, s_port, strerror(errno));
        close(sock);
        sock = -1;
    }
    freeaddrinfo(result);
    if (rp == NULL)
    {
        printf("Connection failed: %s\n", last_error);
        return -1;
    }
    return sock;
}


void do_disconnect(int sock)
{
    if (sock == -1)
        return;
    close(sock);
    return;
}

void usage()
{
    printf("Usage: %s -p port\n", program_name);
}

int main(int argc, char * const argv[])
{
    int  opt;
    int  port = 0; 
    int  ret;
    int  sock = -1;
    int  sock_listen = -1;
    t_test data;
    char *buffer;
    struct sockaddr_in sa;
    printf("%s\n", program_name);
    while ((opt = getopt(argc, argv, "p:")) != -1)
    {
        switch (opt)
        {
            case 'p':
                port = atoi(optarg);
                break;
            default:
                usage();
                return 1;
        }
    }
    if (!port)
    {
        printf("All parameters are required and at least one is missing\n");
        usage();
        return 1;
    }
    sock_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_listen == -1)
    {
        printf("Error creating listening socket: %s\n", strerror(errno));
        return 1;
    }
    int enable = 1;
    ret = setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, 
                     (const char *)&enable, sizeof(enable));
    if (ret == -1)
    {
        printf("Error setting REUSEADDR for listening socket: %s\n", 
               strerror(errno));
        do_disconnect(sock_listen);
        return 1;
    }
#ifdef SO_REUSEPORT
    ret = setsockopt(sock_listen, SOL_SOCKET, SO_REUSEPORT, 
                     (const char *)&enable, sizeof(enable));
    if (ret == -1)
    {
        printf("Error setting REUSEPORT for listening socket: %s\n",
               strerror(errno));
        do_disconnect(sock_listen);
        return 1;
    }
#endif    
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = htons(port);
    if (bind(sock_listen, (const struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
        printf("Error binding port %d to socket: %s\n", port, strerror(errno));
        do_disconnect(sock_listen);
        return 1;
    }
    if (listen(sock_listen, 5) == -1)
    {
        printf("Error listening on socket: %s\n", strerror(errno));
        do_disconnect(sock_listen);
        return 1;
    }
    printf("Now listening\n");
    while ((sock = accept(sock_listen, NULL, NULL)) != -1)
    {
        ret = recv(sock, (char *)&data, sizeof(data), 0);
        if (ret == -1)
        {
            printf("Initial receive error: %s\n", strerror(errno));
            do_disconnect(sock);
            break;
        }
        if (ret != sizeof(data))
        {
            printf("Initial receive wrong size: %d != %d\n", ret, sizeof(data));
            do_disconnect(sock);
            break;
        }
        printf("  %d records, %d bytes, %s validate\n", data.count, data.size, 
               data.validate ? "DO" : "No");
        buffer = malloc(data.size);
        if (!buffer)
        {
            printf("Insufficient memory allocating buffer %d bytes\n", data.size);
            do_disconnect(sock);
            break;
        }
        ret = do_test(sock, buffer, data.size, data.count, data.validate);
        do_disconnect(sock);
        sock = -1;
        free(buffer);
        buffer = NULL;
    }
    do_disconnect(sock_listen);
    return 1;
}
