#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<wait.h>
#include <fcntl.h> 
#define ROCK        0 
#define PAPER       1 
#define SCISSORS    2 

#define STDIN 		0
#define STDOUT 		1
#define STDERR		2

int umpire(char* player1, char* player2, int r){
	// pid_t pid1,pid2;
	// int pfd[4][2];
	// if(pipe(pfd[1])<0)
	// 	return -1; 
	// if(pipe(pfd[0])<0)
	// 	return -1; 
	// pid1=fork();
	// if(pid1<0)
	// 	return -1;
	// if(!pid1){
	// 	dup2(pfd[0][0],0);
	// 	dup2(pfd[1][1],1);
	// 	close(pfd[1][1]);
	// 	close(pfd[0][1]);
	// 	close(pfd[0][0]);
	// 	close(pfd[1][0]);
	// 	if(execl(player1,player1,NULL))
	// 		exit(-1);
	// }
	
	// if(pipe(pfd[3])<0)
	// 	return -1; 
	// if(pipe(pfd[2])<0)
	// 	return -1; 
	// pid2=fork();
	// if(pid2<0)
	// 	return -1;
	// if(!pid2){
	// 	dup2(pfd[2][0],0);
	// 	dup2(pfd[3][1],1);
	// 	close(pfd[2][1]);
	// 	close(pfd[3][1]);
	// 	close(pfd[2][0]);
	// 	close(pfd[3][0]);
	// 	if(execl(player2,player2,NULL))
	// 		exit(-1);
	// }
	// close(pfd[1][1]);
	// close(pfd[0][0]);
	// close(pfd[3][1]);
	// close(pfd[2][0]);
	// int score1=0,score2=0;
	// for(int i=0;i<r;i++){
	// 	char p1[1],p2[1];
	// 	if(write(pfd[0][1],"GO\0",3)!=3)
	// 		return -1;
	// 	if(write(pfd[2][1],"GO\0",3)!=3)
	// 		return -1;
	// 	if(read(pfd[1][0],p1,1)!=1)
	// 		return -1;
	// 	if(read(pfd[3][0],p2,1)!=1)
	// 		return -1;
	// 	if(i==9)close(pfd[0][1]);
	// 	if(i==9)close(pfd[2][1]);	
	// 	if(p1[0]=='0' && p2[0]=='2')score1+=1;
	// 	if(p1[0]=='2' && p2[0]=='0')score2+=1;
	// 	if(p1[0]=='1' && p2[0]=='0')score1+=1;
	// 	if(p1[0]=='0' && p2[0]=='1')score2+=1;
	// 	if(p1[0]=='2' && p2[0]=='1')score1+=1;
	// 	if(p1[0]=='1' && p2[0]=='2')score2+=1;	
	// }
	
	// if(score1>=score2)printf("0");
	// else printf("1");
	printf("0");
	// printf("%d %d",score1,score2);
	return 0;
}
#include "gameUtils.h"

int getWalkOver(int numPlayers); // Returns a number between [1, numPlayers]


int main(int argc, char *argv[])
{
	char* file;
	int rounds;
	if(argc==2){
		file= argv[1];
		rounds= 10;
	}
	else if(argc==4){
		file=argv[3];
		rounds=atoi(argv[2]);
	}
	else
	{
		return -1;
	}
	
	int fd=open(file,O_RDONLY);
	char buf;
	char P_instring[100];
	int index=0;
	while(read(fd,&buf,1)){
		if(buf=='\n')break;
		P_instring[index++]=buf;
	}
	P_instring[index]='\0';
	int P=atoi(P_instring);

	char player[P][101];
	for(int i=0;i<P;i++){
		int index=0; 
		while(read(fd,&buf,1)){
			if(buf=='\n' || buf==EOF)break;
			player[i][index++]= buf;
		}
		player[i][index]='\0';

	}
	int playing[P],pfd[2*P][2];
	for(int i=0;i<P;i++){

		pipe(pfd[2*i]); // Parent Write
		pipe(pfd[2*i+1]); // Chile Read
		pid_t pid=fork();
		if(pid<0)return -1;
		if(!pid){
			dup2(pfd[2*i][0],0);
			dup2(pfd[2*i+1][1],1);
			close(pfd[2*i][1]);
			close(pfd[2*i+1][0]);
			char* arr[]={player[i], NULL};
			execv(player[i],arr);
			exit(-1);
		}
	}
	for(int i=0;i<P;i++)
		playing[i]=i;
	int playN=P;
	while(1){
		int count=0;
		// printf("playN=%d",playN);
		if(playN==1){
			printf("p%d",playing[0]);
			break;
		}
		for(int i=0;i<playN;i++){
			if(count!=playN-1){printf("p%d ",playing[i]);count++;}
			else printf("p%d\n",playing[i]);
		}
		
		
		int walkover=1000000,wnumber=1000000;
		if(playN%2){
			walkover=getWalkOver(playN);
			wnumber=playing[walkover-1];
		}	
		// printf("wlakover =%d\n",wnumber);
		int score[playN];
		for(int i=0;i<playN;i++)
			score[i]=0;
		int matches=playN/2;
		for(int i=0;i<rounds;i++){
			int index=0,p1idx,p2idx;
			for(int j=0;j<matches;j++){
				if(index+1!=walkover)
					p1idx=index++;
				else{
					index++;
					p1idx=index++;
				}
				if(index+1!=walkover)
					p2idx=index++;
				else{
					index++;
					p2idx=index++;
				}
				close(pfd[2*playing[p1idx]+1][1]);
				close(pfd[2*playing[p2idx]+1][1]);
				if(!write(pfd[2*playing[p1idx]][1],"GO\0",3))
					return -1;
				if(!write(pfd[2*playing[p2idx]][1],"GO\0",3))
					return -1;
				char p1[1],p2[1];
				if(read(pfd[2*playing[p1idx]+1][0],p1,1)!=1)
					return -1;
				if(read(pfd[2*playing[p2idx]+1][0],p2,1)!=1)
					return -1;
				if(p1[0]=='0' && p2[0]=='2')score[p1idx]+=1;
				if(p1[0]=='2' && p2[0]=='0')score[p2idx]+=1;
				if(p1[0]=='1' && p2[0]=='0')score[p1idx]+=1;
				if(p1[0]=='0' && p2[0]=='1')score[p2idx]+=1;
				if(p1[0]=='2' && p2[0]=='1')score[p1idx]+=1;
				if(p1[0]=='1' && p2[0]=='2')score[p2idx]+=1;

			}
		}
		int index=0,p1idx,p2idx,k=0,flag=0;
		for(int j=0;j<matches;j++){
			if(index+1!=walkover)
					p1idx=index++;
			else{
				playing[k++]=wnumber;
				index++;
				flag=1;
				p1idx=index++;
			}
			if(index+1!=walkover)
				p2idx=index++;
			else{
				playing[k++]=wnumber;
				flag=1;
				index++;
				p2idx=index++;
			}
			if(score[p1idx]>=score[p2idx])
				playing[k++]=playing[p1idx];
			else
				playing[k++]=playing[p2idx];			
		}
		if(wnumber!=1000000 && flag==0)
			playing[k++]=wnumber;
		playN = playN/2+playN%2;
	}
	int status;
	for(int i=0;i<P;i++){
		// wait(&status);
		// if(status)return -1;
		close(pfd[2*i][1]);
	}
	return 0;
}
