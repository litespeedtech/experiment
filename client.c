#include "do_test.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
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
    printf("Usage: %s -a ip_address -p port -c count_of_records "
           "-s each_record_size [-v]\n", program_name);
}

#define STR_NS_SIZE 256
char *str_long(long l, char str[])
{
    char part[STR_NS_SIZE];
    str[0] = 0;
    while (l >= 1000)
    {
        snprintf(part, STR_NS_SIZE, ",%03ld%s", l % 1000, str);
        strcpy(str, part);
        l /= 1000;
    }
    
    snprintf(part, STR_NS_SIZE, "%d%s", l, str);
    strcpy(str, part);
    return str;
}

char *str_ns(long ns, char str[])
{
    char strlong[STR_NS_SIZE];
    snprintf(str, STR_NS_SIZE, "%s.%03ld,%03ld,%03ld",
             str_long(ns / 1000000000l, strlong),   // secs
             (ns % 1000000000l) / 1000000l,         // ms
             (ns % 1000000000l) / 1000 % 1000,      // us
             (ns % 1000l));                         // ns
    return str;
}

int main(int argc, char * const argv[])
{
    int   opt;
    long  count = 0;
    long  size = 0;
    char *address = NULL;
    int   port = 0; 
    int   validate = 0;
    int   ret;
    int   sock;
    struct timespec ts;
    int64_t start, end;
    t_test data;
    char *buffer;
    
    printf("%s\n", program_name);
    while ((opt = getopt(argc, argv, "c:s:a:p:v")) != -1)
    {
        switch (opt)
        {
            case 'c':
                count = atol(optarg);
                break;
            case 's':
                size = atol(optarg);
                break;
            case 'a':
                address = strdup(optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'v':
                printf("Validation enabled\n");
                validate = 1;
                break;
            default:
                usage();
                return 1;
        }
    }
    if ((!count) || (!size) || (!address) || (!port))
    {
        printf("All parameters are required and at least one is missing\n");
        usage();
        return 1;
    }
    // Do the buffer allocation, initialization and other stuff outside the timer
    buffer = malloc(size);
    if (!buffer)
    {
        printf("Insufficient memory to allocation %d bytes\n", size);
        return 1;
    }
    long i;
    char *buf = buffer;
    for (i = 0; i < size; ++i)
    {
        (*buf) = (char)i;
        buf++;
    }
    if ((sock = do_connect(address, port)) == -1)
    {
        free(buffer);
        return 1;
    }
    data.count = count;
    data.size = size;
    data.validate = validate;
    if (send(sock, &data, sizeof(data), 0) != sizeof(data))
    {
        printf("Error sending details to partner: %s\n", strerror(errno));
        do_disconnect(sock);
        free(buffer);
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &ts);
    start = ts.tv_sec * 1000000000 + ts.tv_nsec;
    ret = do_test(sock, buffer, size, count, validate);
    do_disconnect(sock);

    clock_gettime(CLOCK_MONOTONIC, &ts);
    end = ts.tv_sec * 1000000000 + ts.tv_nsec;
    free(buffer);
    if (ret == -1)
        return 1;
    long KB_sec;
    if (end - start < 1000000)
        KB_sec = 0;
    else
        KB_sec = ((size * count)) / ((end - start) / 1000000); // KB/ns
    char str_time[STR_NS_SIZE];
    char str_chars_sec[STR_NS_SIZE];
    char str_chars[STR_NS_SIZE];
    printf("Total time: %ld ns, chars: %ld MB/sec %ld\n", end - start, size * count, KB_sec);
    printf("Total time: %s seconds chars: %s KB/sec: %s\n", 
           str_ns(end - start, str_time),
           str_long(size * count, str_chars),
           str_long(KB_sec, str_chars_sec));
    return 0;
}
