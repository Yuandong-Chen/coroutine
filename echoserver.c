#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <sys/resource.h> /*setrlimit */
#include "ezco.h"

#define EHCO_PORT    8080
#define MAXEPOLLSIZE 2000 /*maximum concurrent connections*/
int closing =0;
Thread_t *reat;
void *handler(void *clientfd)
{
        char buff[101];
        struct pollfd pl[2];
        int n;
        int err;
        pl[0].fd = clientfd;
        pl[1].fd = clientfd;
        pl[0].events = POLLIN;
        pl[1].events = POLLOUT;

    _RETRY:

        while((err = poll(pl, 1, 0)) > 0){
            if(pl[0].revents & POLLIN)
            {
                n = read((int)clientfd,buff, 100);

                if(n <= 0)
                	goto _CLOSE;

                buff[n] = '\0';
                if(strncmp(buff, "quit",4) == 0)
                {
                    close((int)clientfd);
                    break;
                }
                else if(strncmp(buff, "close",5) == 0)
                {
                        //server closing
                    closing = 1;
                    fprintf(stderr, "server is closing\n");
                    break;
                }

            _REWRITE:
                if((err = poll(&pl[1], 1, 0)) > 0){
                    if(pl[1].revents & POLLOUT)
                    {
                        n = write((int)clientfd,buff,  n);
			
                        if(n <= 0)
                		goto _CLOSE;
                    }
                    else
                    {
                        goto _CLOSE;
                    }
                }

                if(err == 0){
                    switch_to(0);
                    goto _REWRITE;
                }
            }
            else
            {
                goto _CLOSE;
            }   
        }

        if(err == 0){
            switch_to(0);
            goto _RETRY;
        }

    _CLOSE:
	fprintf(stderr, "client close the connection\n");
        close((int)clientfd);
        return NULL;
}

int main()
{
    initez();
    int socketfd, acceptCount = 0;
    int buf[256];
    Thread_t *tid;
    struct rlimit rt;

    /* 设置每个进程允许打开的最大文件数 */
    rt.rlim_max = rt.rlim_cur = MAXEPOLLSIZE;
    if (setrlimit(RLIMIT_NOFILE, &rt) == -1) 
    {
        perror("setrlimit error");
        return -1;
    }
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
       
    if(socketfd == -1)
    {
        fprintf(stderr, "errno=%d\n", errno);
        exit(1);
    }
    else
    {
        fprintf(stderr, "socket create successfully\n");
    }

    struct sockaddr_in sa;
    bzero(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(EHCO_PORT);
    sa.sin_addr.s_addr = htons(INADDR_ANY);
    bzero(&(sa.sin_zero), 8);
    int opt=1;
    setsockopt(socketfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if(bind(socketfd, (struct sockaddr *)&sa, sizeof(sa))!= 0)
    {
        fprintf(stderr, "bind failed\n");
        fprintf(stderr, "errno=%d\n", errno);
        exit(1);
    }
    else
    {
        fprintf(stderr, "bind successfully\n");
    }

    //listen
    int val = fcntl(socketfd, F_GETFL, 0);
    fcntl(socketfd, F_SETFL, val | O_NONBLOCK);

    if(listen(socketfd ,SOMAXCONN) != 0)
    {
        fprintf(stderr, "listen error\n");
        exit(1);
    }
    else
    {
        fprintf(stderr, "listen successfully on port 8080\n");
    }

    int clientfd;
    struct sockaddr_in clientAdd;
    socklen_t len = sizeof(clientAdd);
    threadCreat(&reat, destroy, &dead);
    
_REDO:
    switch_to(0);
    if(closing)
        goto _CLOSE;
    
    while((clientfd = accept(socketfd, (struct sockaddr *)&clientAdd, &len)) >0 )
    {
	sprintf(buf, "accept form %s:%d\n", inet_ntoa(clientAdd.sin_addr), clientAdd.sin_port);
	fprintf(stderr,"%d:%s", ++acceptCount, buf);
        threadCreat(&tid, handler, clientfd);
        switch_to(0);
        if(closing)
            goto _CLOSE;
    }
    goto _REDO;

_CLOSE:
    close(socketfd);
    endez();
    return 0;
}
