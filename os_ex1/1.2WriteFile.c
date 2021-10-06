#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void write_file(int fd, const char *infile){
	int fd2;
	char buff[1024];
	intt rcnt = 1;
	fd2 = open(infile, O_RDONLY);
	if (fd2 == -1){
		//printf("%s: No such file or directory\n", infile);
		perror(infile);
		exit(1);
	}
	while (rcnt != 0) {
		rcnt = read(fd2, buff, sizeof(buff)-1);
		if (rcnt == -1){ /* error */
			perror("read");
			return;
		}	
		if (rcnt != 0 ){
			buff[rcnt] = '\0';
			doWrite(fd, buff, rcnt + 1);
		}
	}
	close(fd2);
}
