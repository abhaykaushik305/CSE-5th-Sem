#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
int main(void){
	printf("current pid is = %d\n",getpid());
	if(execl("./2.out","2.out",NULL))
		perror("exec");
	printf("ye to dikhave ke liye kr rha main");
	return 0;
}
