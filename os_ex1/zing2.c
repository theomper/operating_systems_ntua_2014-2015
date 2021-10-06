#include <unistd.h>
#include <stdio.h>

int zing(){
	printf("Hello %s!!!!!\n", getlogin());
	return 0;
}