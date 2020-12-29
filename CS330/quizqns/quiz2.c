#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
int main(void)
{
  int fd, i; 
  for(i=0; i<2; ++i){
        int pfd[2];
	fork();  
        pipe(pfd);
  }
}
/*
 * 1. How many pipes the above program will create?
 * 2. Let us define R to be the number of processes that refer to any given pipe P.
 *    What is the maximum value of R across all pipes created during the execution 
 *    of the above program?
 */
