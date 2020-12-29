#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<wait.h>
int main()
{
    int i,status;
    pid_t pid,cpid;
    printf("Before calling fork mypid = %d\n", getpid());
    
    pid = fork();
    if(pid < 0){
         perror("fork");
         exit(-1);
    }
    
    if(!pid){ /*Child*/
         printf("%s: Before calling exec mypid = %d\n", __FILE__, getpid());
         if(execl("./2.out", "2.out", NULL))
                 perror("exec");
         exit(-1);
    }
   cpid= wait(&status);
   for(i=0; i<5; ++i){
       	//sleep(1);
        printf("%s After calling exec mypid = %d\n", __FILE__, getpid());
   }
}
