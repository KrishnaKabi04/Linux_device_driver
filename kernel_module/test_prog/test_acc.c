#include <blockmma.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <malloc.h>
#include <sys/syscall.h>
#include <sys/prctl.h> // prctl(), PR_SET_PDEATHSIG
#include <signal.h> // signals
#include <sys/socket.h>
#include <netinet/in.h>

#define RCVBUFSIZE 256

int main(int argc, char *argv[])
{
    // variable initialization
    FILE *fp;
    int devfd;
    char *authors;
    struct blockmma_hardware_cmd cmd;
    int pagesize = sysconf(_SC_PAGE_SIZE);

    devfd = open("/dev/blockmma", O_RDWR);
    if (devfd < 0)
    {
        fprintf(stderr, "Device open failed");
        exit(1);
    }
    authors = (char *)memalign(pagesize, pagesize); // function returns a block of memory of size bytes aligned to blocksize. 
                                                    //The blocksize must be given as a power of two. It sets errno and 
                                                    //returns a null pointer upon failure.
    cmd.op = (__u64)0;
    cmd.a = (__u64)authors;
    
    ioctl(devfd, BLOCKMMA_IOCTL_AUTHOR, &cmd);
    fprintf(stderr, "Authors: %s\n",authors);
    return 0;
}