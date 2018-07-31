#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <linux/tcp.h>
#include "../do_test.h"

// All real programs are required to provide the following:
const char *program_name = "splice-client";

int do_test(int sock, char *buffer, long size, long count, int validate)
{
    long record;
    int pipes[2];
    int   busy_waits = 0;
    int   total_writes = 0;
    int  *p_header = (int *)buffer;
    int  *p_trailer = (int *)(buffer + size - 1 - sizeof(int));
    int   fatal = 0;
    int   maxseg = 0;
    int   m_buffer_size = 1048576;
    int   m_num_buffers = m_buffer_size / size;
    uint64_t m_start_offset = 0;
    int   m_record_stop = 0;
    int   extra_calls = 0;
    int   sleeps = 0;
    int   buf_num;
    char *m_buffer[m_num_buffers];
    int   i;
    
    if (pipe(pipes))
    {
        printf("Error: %s creating anonymous pipe\n", strerror(errno));
        return -1;
    }
    {
        int len = sizeof(maxseg);
        if (getsockopt(sock, 6 /* TCP */, TCP_MAXSEG, (char *)&maxseg, &len) == -1)
            printf("Error getting max segsize: %d\n", errno);
        else 
            printf("MaxSeg: %d\n", maxseg);
    }
    if (m_buffer_size < size)
        m_buffer_size = size;
    for (buf_num = 0; buf_num < m_num_buffers; ++buf_num)
    {
        m_buffer[buf_num] = mmap(NULL, m_buffer_size, PROT_READ | PROT_WRITE, 
                                 MAP_SHARED | MAP_ANON, -1, 0);
        if (!m_buffer)
        {
            printf("Error doing mmap to get mapped data: %d\n", errno);
            for (i = 0; i < buf_num; ++i)
                munmap(m_buffer[buf_num], size);
            close(pipes[0]);
            close(pipes[1]);
            return -1;
        }
    }
    
    for (record = 0; record < count; ++record)
    {
        // Write the data to the pipe
        ssize_t bytes;
        struct iovec io;
        // give it time to catch up from time to time
        while (record == m_record_stop) {
            struct tcp_info info;
            extra_calls++;
            int len = sizeof(struct tcp_info);
            if (getsockopt(sock, 6 /* TCP */, TCP_INFO, (char *)&info, 
                &len) == -1)
            {
                printf("getsockopt error #%d\n", errno);
                fatal = 1;
                break;
            }
            if (record == 0)
                m_start_offset = info.tcpi_bytes_acked;
            m_record_stop = (int)((m_buffer_size - ((uint64_t)(record * size) + m_start_offset - info.tcpi_bytes_acked)) / (uint64_t)size);
            m_record_stop += record;
            //printf("Extra call, record #%d, m_record_stop: %d, acked: %lu, start_offset: %lu\n",
            //       record, m_record_stop, info.tcpi_bytes_acked, m_start_offset);
            if (record == m_record_stop)
            {
                usleep(0);
                sleeps++;
            }
        }
        if (validate)
        {
            (*p_header) = record;
            (*p_trailer) = record;
        }
        
        //printf("Write a record #%d, log_rec: %d\n", record, record % m_num_buffers);
        memcpy(m_buffer[record % m_num_buffers], buffer, size);
        io.iov_base = m_buffer[record % m_num_buffers];
        io.iov_len = size;
        bytes = vmsplice(pipes[1], &io, 1, SPLICE_F_GIFT | SPLICE_F_MOVE);
        if (bytes == -1)
        {
            printf("Error vmsplice to the pipe: %s\n", strerror(errno));
            fatal = 1;
            break;
        }
        //printf("Wrote %ld bytes to pipe\n", (long)bytes);
        long bytesInPipe = size;
        while (bytesInPipe)
        {
            bytes = splice(pipes[0], NULL, sock, NULL, (size_t)bytesInPipe,
                           (unsigned int)(SPLICE_F_MORE | SPLICE_F_MOVE));
            int old_errno = errno;
            //printf("spliced, %ld bytes, errno: %d\n", (long)bytes, errno);
            errno = old_errno;
            if (bytes <= 0)
            {
                if (errno == EINTR || errno == EAGAIN)
                {
                    // Interrupted system call, try again.
                    busy_waits++;
                    continue;
                }
                printf("Splice error: #%d\n", errno);
                fatal = 1;
                return -1;
            }
            bytesInPipe -= bytes;
            total_writes++;
        }
        if (fatal)
            break;
    }
    close(pipes[0]);
    close(pipes[1]);
    for (i = 0; i < m_num_buffers; ++i)
        munmap(m_buffer, size);
    
    printf("%d busy_waits, %d total writes, %d extra calls, %d sleeps\n", 
           busy_waits, total_writes, extra_calls, sleeps);
    if (fatal)
    {
        printf("FATAL error reported above\n");
        return -1;
    }
    return 0;
}
        


