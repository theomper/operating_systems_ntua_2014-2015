#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void doWrite(int fd, const char *buff, int len){
	ssize_t wcnt;
	int remain = len;
	int idx = 0;
	do {
		wcnt = write(fd, buff + idx, remain);
		if (wcnt < 0){ /* error */
			perror("write");
			return;
		}
		idx += wcnt;
		remain -= idx;
	} while (idx < len);
	return;
}
