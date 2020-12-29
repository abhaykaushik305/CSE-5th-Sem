#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<wait.h>
#include <fcntl.h> 
int executeCommand (char *cmd) {
	 pid_t pid,pid2,cpid;
	 pid = fork();
	// if(pid<0){
	// 	printf("UNABLE TO EXECUTE\n");
	// 	return -1;	
	// }
	
	if(!pid){
		char *arr[10];
		char* token = strtok(cmd," \n");
		arr[0] = token;
		int i=0;    
		while (token != NULL) { 
			token= strtok(NULL, " \n");
			arr[++i]=token; 
		}
		char *arr2[i+1];
		for(int j=0;j<=i;j++)
			arr2[j]=arr[j];			
		char *path= getenv("CS330_PATH");
		char *token2 = strtok(path,":");    
    	while (token2 != NULL) { 
				pid2=fork();
				if(!pid2){
					strcat(token2,"/");
					strcat(token2,arr2[0]);
					execv(token2,arr2);
					exit(-1);
				}
        		// printf("%s\n", token2); 
        		int found;
				wait(&found);
				if(!found)return 0;
				token2 = strtok(NULL, ":"); 
    	}
		printf("UNABLE TO EXECUTE\n");
		exit(-1); 	
	}
	else{
	int status;
	cpid = wait(&status);
	if(!status)return -1;
	return 0;
	}
}

int execute_in_parallel(char *infile, char *outfile)
{
	int fd1, fd2,status,ret=0;
	// char buf[32];
	int pfd[50][2];
	fd1 = open(infile, O_RDONLY);
	dup2(fd1,0);
	char cmd[101];
	char cmds[50][101];
	int i=0;
	while(fgets(cmd,101,stdin)!=NULL){
		if(cmd[0]!='\n'){
			strcat(cmds[i],cmd);
			i++;
		}
	}
	int len=i;
	// printf("%d\n",len);
	for(int j=0;j<len;j++){
		if(pipe(pfd[j])<0){
			ret=-1;
			continue;
		}
		pid_t pid=fork();
		if(pid<0){
			ret =-1;
			continue;
		}
		if(!pid){
			dup2(pfd[j][1],1);
			// printf("%s",cmds[j]);
			if(executeCommand(cmds[j])==-1)exit(-1);
			return 0;
		}
   	}
   	fd2=open(outfile,O_RDWR | O_CREAT, 0644);
	dup2(fd2,1);
 	for(int j=0;j<len;j++){

		wait(&status);
		if(!status)ret=-1;
		int buf_size=5000;
		char buf[buf_size];
		int x;
		// while((x=read(pfd[j][0],buf,buf_size)) > 0){
		// 	printf("%s",buf);
		// 	if(x<buf_size)break;			
		// }
			
		x=read(pfd[j][0],buf,buf_size);
		buf[x]='\0';
		printf("%s",buf);
// 		}
// 		
  	}

	return ret;
}

int main(int argc, char *argv[])
{
	return execute_in_parallel(argv[1], argv[2]);
}
