#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
static int num1=0,num2=0;
main(){
	pid_t pid =getpid();
       int retval=1;
	for(int i=0;i<2;i++){
		num1++;
		retval=fork();
		if(retval!=0)
			num2++; 
	}
 	printf("num1= %d num2= %d\n",num1,num2);
	if(getpid()==pid)
		wait(NULL);	
}

