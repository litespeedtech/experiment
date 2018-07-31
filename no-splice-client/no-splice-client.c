#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include "../do_test.h"

// All real programs are required to provide the following:
const char *program_name = "no-splice-client";

int do_test(int sock, char *buffer, long size, long count, int validate)
{
    long record;
    int  *p_header = (int *)buffer;
    printf("buffer = %p, p_header = %p, sock: %d\n", buffer, p_header, sock);
    int  *p_trailer = (int *)(buffer + size - 1 - sizeof(int));
    for (record = 0; record < count; ++record)
    {
        if (validate)
        {
            (*p_header) = record;
            (*p_trailer) = record;
        }
        int rc = send(sock, buffer, (int)size, 0);
        if (rc != size)
        {
            if (rc == -1)
                printf("Comm error %d at record #%d, sock: %d\n", errno, record, sock);
            else 
                printf("Unexpected send rc: %d\n", rc);
            return -1;
        }
    }
    return 0;
}
        


