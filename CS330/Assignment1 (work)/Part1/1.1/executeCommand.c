#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<wait.h>
int executeCommand (char *cmd) {
	 pid_t pid,pid2,cpid;
	 pid = fork();
	// if(pid<0){
	// 	printf("UNABLE TO EXECUTE\n");
	// 	exit(-1);	
	// }	
	if(!pid){
		char *arr[strlen(cmd)];
		char* token = strtok(cmd," ");
		arr[0] = token;
		int i=0;    
		while (token != NULL) { 
			token= strtok(NULL, " ");
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
					if(execv(token2,arr2)){
						exit(-1);
					}
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
	
	int status;
	cpid = wait(&status);
	if(status)return -1;
	return WEXITSTATUS(status);
	
}

int main (int argc, char *argv[]) {
	return executeCommand(argv[1]);
}
