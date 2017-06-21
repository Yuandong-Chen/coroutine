
/************************* Test *************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <aio.h>
#include <errno.h>
#include <string.h>
#include "../ezco.h"

#include <sys/types.h>      /* some systems still require this */
#include <sys/stat.h>

Thread_t *tid[8];
int ifd, ofd;

unsigned char translate(unsigned char c)
{
    if (isalpha(c)) {
        if (c >= 'n')
            c -= 13;
        else if (c >= 'a')
            c += 13;
        else if (c >= 'N')
            c -= 13; 
        else
            c += 13;
    }
    return(c); 
}

int asyncread(int ifd, char buf[], off_t offset, size_t nbytes)
{
    struct aiocb aiocb;
    bzero(&aiocb,sizeof(aiocb));
    int err;
    Thread_t *pth = live.current->data;
    aiocb.aio_buf = buf;
    aiocb.aio_sigevent.sigev_notify = SIGEV_NONE;
    aiocb.aio_fildes = ifd;
    aiocb.aio_offset = offset;
    aiocb.aio_nbytes = nbytes;
    err = aio_read(&aiocb);
    if(err < 0){
        perror("aio_read error");
        return err;
    }

    ezco_yield(pth, 0, (err = aio_error(&aiocb)) != EINPROGRESS);

    if(err != 0){
        perror("aio_read error after read");
        exit(1);
    }

    return aio_return(&aiocb);
}

int asyncwrite(int ofd, char buf[], off_t offset, size_t nbytes)
{
    struct aiocb aiocb;
    bzero(&aiocb,sizeof(aiocb));
    int err;
    Thread_t *pth = live.current->data;
    aiocb.aio_buf = buf;
    aiocb.aio_sigevent.sigev_notify = SIGEV_NONE;
    aiocb.aio_fildes = ofd;
    aiocb.aio_offset = offset;
    aiocb.aio_nbytes = nbytes;
    err = aio_write(&aiocb);
    if(err < 0){
        perror("aio_write error");
        return err;
    }

    ezco_yield(pth, 0, (err = aio_error(&aiocb)) != EINPROGRESS);

    if(err != 0){
        perror("aio_read error after write");
        exit(1);
    }

    return aio_return(&aiocb);
}

void *myEncrypt(void *d)
{
    char buf[BUFSIZ];
    int i;
    int n = asyncread(ifd, buf, (off_t)d, BUFSIZ);
    if(n < 0)
        exit(1);
    for (i = 0; i < n; ++i)
    {
    	buf[i] = translate(buf[i]);
    }
    if(asyncwrite(ofd, buf, d, n) <0 )
        exit(1);
}

void check(int fd)
{
    struct stat sbuf;
    char *ptr;

    if (fstat(fd, &sbuf) < 0){
        perror("fstat failed");
        exit(1);
    }


    if(!S_ISREG(sbuf.st_mode)){
        fprintf(stderr, "not regular file\n");
        if (S_ISDIR(sbuf.st_mode))
            ptr = "directory";
        else if (S_ISCHR(sbuf.st_mode))
            ptr = "character special";
        else if (S_ISBLK(sbuf.st_mode))
            ptr = "block special";
        else if (S_ISFIFO(sbuf.st_mode))
            ptr = "fifo";
        else if (S_ISLNK(sbuf.st_mode))
            ptr = "symbolic link";
        else if (S_ISSOCK(sbuf.st_mode))
            ptr = "socket";
        else
            ptr = "** unknown mode **";
        fprintf(stderr, "%s\n", ptr); 
        exit(1);
    }
}

int main(int argc, char const *argv[])
{
    
    int i, j, num = 0;
    off_t off = 0;
    if(argc != 3)
    {
        fprintf(stderr, "usage: prog infile outfile\n");
        exit(1);
    }
    ifd = open(argv[1], O_RDONLY);
    ofd = open(argv[2], O_RDWR|O_CREAT, 0664);
    check(ifd);
    check(ofd);
    
    initez();

    struct stat sbuf;
    fstat(ifd, &sbuf);
    num = sbuf.st_size / BUFSIZ;
    fprintf(stderr, "create %d coroutines\n", num + 1);

    for (j = 0; j < (num+1)/8; ++j)
    {
        /* code */
        for (i = 0; i < 8 ; ++i)
        {
            threadCreat(&tid[i],myEncrypt,off);  
            off += BUFSIZ; 
        }
        for(i = 0; i< 8 ; i++){
            threadJoin(tid[i], NULL);
        }
    }

    for (i = 0; i < (num+1)%8; ++i)
    {
        threadCreat(&tid[i],myEncrypt,off);  
        off += BUFSIZ; 
    }

    for(i = 0; i< (num+1)%8; i++){
        threadJoin(tid[i], NULL);
    }
    
    endez();

    return 0;          
}
