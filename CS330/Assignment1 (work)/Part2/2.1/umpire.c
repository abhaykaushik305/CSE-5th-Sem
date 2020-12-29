#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<wait.h>
#include <fcntl.h> 

int main(int argc, char* argv[]) {
	if(argc!=3)
		return -1;
	pid_t pid1,pid2;
	int pfd[4][2];
	// int efd1[2],efd2[2];
	if(pipe(pfd[1])<0)
		return -1; 
	if(pipe(pfd[0])<0)
		return -1; 
	// if(pipe(efd1)<0)
	// 	return -1;
	pid1=fork();
	if(pid1<0)
		return -1;
	if(!pid1){
		dup2(pfd[0][0],0);
		dup2(pfd[1][1],1);
		// dup2(efd1[1],2);
		// close(efd1[0]);
		close(pfd[1][1]);
		close(pfd[0][1]);
		close(pfd[0][0]);
		close(pfd[1][0]);
		if(execl(argv[1], argv[1],NULL))
			exit(-1);
	}
	// if(pipe(efd2)<0)
	// 	return -1;
	if(pipe(pfd[3])<0)
		return -1; 
	if(pipe(pfd[2])<0)
		return -1; 
	pid2=fork();
	if(pid2<0)
		return -1;
	if(!pid2){
		dup2(pfd[2][0],0);
		dup2(pfd[3][1],1);
		// dup2(efd1[1],2);
		// close(efd2[0]);
		close(pfd[2][1]);
		close(pfd[3][1]);
		close(pfd[2][0]);
		close(pfd[3][0]);
		if(execl(argv[2],argv[2],NULL)){
			// write(efd2)
			exit(-1);
		}
	}
	close(pfd[1][1]);
	close(pfd[0][0]);
	close(pfd[3][1]);
	close(pfd[2][0]);
	// close(efd1[1]);
	// close(efd2[1]);
	int score1=0,score2=0;
	for(int i=0;i<10;i++){
		char p1[1],p2[1];
		if(write(pfd[0][1],"GO\0",3)!=3)
			return -1;
		if(write(pfd[2][1],"GO\0",3)!=3)
			return -1;
		char c;
		// if(read(efd1[0],&c,1)==1)
		// 	return -1;
		// if(read(efd2[0],&c,1)==1)
		// 	return -1;
		if(read(pfd[1][0],p1,1)!=1)
			return -1;
		if(read(pfd[3][0],p2,1)!=1)
			return -1;
		if(i==9)close(pfd[0][1]);
		if(i==9)close(pfd[2][1]);	
		// printf("%c %c\n",p1[0],p2[0]);
		if(p1[0]=='0' && p2[0]=='2')score1+=1;
		if(p1[0]=='2' && p2[0]=='0')score2+=1;
		if(p1[0]=='1' && p2[0]=='0')score1+=1;
		if(p1[0]=='0' && p2[0]=='1')score2+=1;
		if(p1[0]=='2' && p2[0]=='1')score1+=1;
		if(p1[0]=='1' && p2[0]=='2')score2+=1;
		// printf("scores== %d %d\n",score1,score2);	
	}
	printf("%d %d",score1,score2);
	return 0;
}
