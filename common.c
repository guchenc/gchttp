#include "common.h"

void make_nonblocking(int fd) 
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

void assertNotNULL(void* p)
{
    assert(p != NULL);
}
