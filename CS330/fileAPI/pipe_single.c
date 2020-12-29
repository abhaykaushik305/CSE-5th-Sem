#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
int main(void)
{


 for(int i=0;i<2;++i){
	int pfd[2];
	fork();
	pipe(pfd);
	}		
}
