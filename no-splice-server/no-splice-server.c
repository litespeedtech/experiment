#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include "../do_test.h"

// All real programs are required to provide the following:
const char *program_name = "no-splice-server";

int do_test(int sock, char *buffer, long size, long count, int validate)
{
    long record;
    int  errors = 0;
    int  *p_header = (int *)buffer;
    int  *p_trailer = (int *)(buffer + size - 1 - sizeof(int));
    
    for (record = 0; record < count; ++record)
    {
        long total = 0;
        while (total < size)
        {
            int rc = recv(sock, &buffer[total], (int)(size - total), 0);
            if (rc <= 0)
            {
                if (rc == 0)
                    printf("recv detected closed socket, on record #%d\n", record);
                else 
                    printf("recv error %d on record #%d\n", errno, record);
                return -1;
            }
            total += rc;
        }
        if (validate)
        {
            if (*p_header != record)
            {
                printf("Invalid record header, record #%d %u!=%d\n", record,
                       *p_header, record);
                return -1;
            }
            if (*p_trailer != record)
            {
                printf("Invalid record trailer, record #%d %u!=%d\n", record,
                       *p_trailer, record);
                return -1;
            }
        }
    }
    printf("Completed receive of %d records \n", count);
    return 0;
}
        


