#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>      /* some systems still require this */
#include <sys/stat.h>
ssize_t             /* Read "n" bytes from a descriptor  */
readn(int fd, void *ptr, size_t n)
{
	size_t		nleft;
	ssize_t		nread;

	nleft = n;
	while (nleft > 0) {
		if ((nread = read(fd, ptr, nleft)) < 0) {
			if (nleft == n)
				return(-1); /* error, return -1 */
			else
				break;      /* error, return amount read so far */
		} else if (nread == 0) {
			break;          /* EOF */
		}
		nleft -= nread;
		ptr   += nread;
	}
	return(n - nleft);      /* return >= 0 */
}

ssize_t             /* Write "n" bytes to a descriptor  */
writen(int fd, const void *ptr, size_t n)
{
	size_t		nleft;
	ssize_t		nwritten;

	nleft = n;
	while (nleft > 0) {
		if ((nwritten = write(fd, ptr, nleft)) < 0) {
			if (nleft == n)
				return(-1); /* error, return -1 */
			else
				break;      /* error, return amount written so far */
		} else if (nwritten == 0) {
			break;
		}
		nleft -= nwritten;
		ptr   += nwritten;
	}
	return(n - nleft);      /* return >= 0 */
}


#define BSZ 1

unsigned char buf[BSZ];

unsigned char
translate(unsigned char c)
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

int
main(int argc, char* argv[])
{
	int	ifd, ofd, i, n, nw;
	struct stat         sbuf;


	if (argc != 3){
		perror("usage: rot13 infile outfile");
		exit(1);
	}
		
	if ((ifd = open(argv[1], O_RDONLY)) < 0){
		fprintf(stderr, "can't open %s", argv[1]);
		exit(1);
	}
		
	if ((ofd = open(argv[2], O_RDWR|O_CREAT, 0664)) < 0){
		fprintf(stderr,"can't create %s", argv[2]);
		exit(1);
	}

	while ((n = readn(ifd, buf, BSZ)) > 0) {
		if (fstat(0, &sbuf) < 0){
			fprintf(stderr,"fstat failed");
			exit(1);
		}

    	//fprintf(stderr,"sbufsize: %d\n", sbuf.st_size);
		for (i = 0; i < n; i++)
			buf[i] = translate(buf[i]);
		if ((nw = writen(ofd, buf, n)) != n) {
			if (nw < 0){
				fprintf(stderr,"write failed");
				exit(1);
			}
			else{
				fprintf(stderr,"short write (%d/%d)", nw, n);
				exit(1);
			}
		}
	}

	fsync(ofd);
	exit(0);
}
