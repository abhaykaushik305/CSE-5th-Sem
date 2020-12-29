#include <msg_queue.h>
#include <context.h>
#include <memory.h>
#include <file.h>
#include <lib.h>
#include <entry.h>



/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/

struct msg_queue_info *alloc_msg_queue_info()
{
	struct msg_queue_info *info;
	info = (struct msg_queue_info *)os_page_alloc(OS_DS_REG);
	
	if(!info){
		return NULL;
	}
	return info;
}

void free_msg_queue_info(struct msg_queue_info *q)
{
	os_page_free(OS_DS_REG, q);
}

struct message *alloc_buffer()
{
	struct message *buff;
	buff = (struct message *)os_page_alloc(OS_DS_REG);
	if(!buff)
		return NULL;
	return buff;	
}

void free_msg_queue_buffer(struct message *b)
{
	os_page_free(OS_DS_REG, b);
}

/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/


int do_create_msg_queue(struct exec_context *ctx)
{
	/** 
	 * TODO Implement functionality to
	 * create a message queue
	 **/
	u32 mypid=ctx->pid;
	int iter=3;
	while(iter<MAX_OPEN_FILES && ctx->files[iter])
		iter++;
	if(iter==MAX_OPEN_FILES)
		return -EINVAL;
	struct file* fp=alloc_file();
	if(fp==NULL)
		return -ENOMEM;
	// from question 
	fp->fops->read=NULL;
	fp->fops->write=NULL;
	fp->fops->close=NULL;
	fp->fops->lseek=NULL;
	struct msg_queue_info *msgqinfo= alloc_msg_queue_info();
	if(msgqinfo==NULL)
		return -ENOMEM;
	// block matrix for capturing blocks from i to j
	for(int i=0;i<MAX_MEMBERS;i++){
		for(int j=0;j<MAX_MEMBERS;j++){
			msgqinfo->block_id[i][j]=0;
		}
	}
	struct message* msg_buff=alloc_buffer();
	if(msg_buff==NULL)
		return -ENOMEM;
	//Initialsion of 
	int iter2=0;
	while(iter2<128){
		msg_buff[iter2].from_pid=0;
		msg_buff[iter2].to_pid=0;
		iter2++;
	}
	msgqinfo->msg_buffer=msg_buff;
	msgqinfo->members.member_pid[0]=mypid;
	msgqinfo->members.member_count=1;
	fp->msg_queue=msgqinfo;
	//  Initalise pids
	iter2=1;
	while(iter2<MAX_MEMBERS)
		msgqinfo->members.member_pid[iter2++] = 0;
	
	ctx->files[iter]=fp;
	return iter;
}


int do_msg_queue_rcv(struct exec_context *ctx, struct file *filep, struct message *msg)
{
	/** 
	 * TODO Implement functionality to
	 * recieve a message
	 **/
	
	if(filep ==NULL || filep->msg_queue == NULL)
		return -EINVAL;
	struct msg_queue_info* msgqinfo=filep->msg_queue;
	struct msg_queue_member_info members_queue = msgqinfo->members;
	int rec_id=-1;
	for(int i=0;i<MAX_MEMBERS;i++)
		if(members_queue.member_pid[i]==ctx->pid)
			rec_id=i;
	if(rec_id == -1)
		return -EINVAL;
	if(msgqinfo->msg_buffer[rec_id*32].from_pid){
		// extract first element element from queue
		*msg = msgqinfo->msg_buffer[rec_id*32];

		// delete first element from queue shift all other elemnts

		int iter=0;
		while(iter!=32){
			if(iter==31)
				msgqinfo->msg_buffer[rec_id*32+iter].from_pid=0;
			else
				msgqinfo->msg_buffer[rec_id*32+iter]=msgqinfo->msg_buffer[rec_id*32+iter+1];
			
			iter++;
		}
		return 1;
	}		
	else{
		return 0;
	}
	return -EINVAL;
}


int do_msg_queue_send(struct exec_context *ctx, struct file *filep, struct message *msg)
{
	/** 
	 * TODO Implement functionality to
	 * send a message
	 **/
	if(filep ==NULL || filep->msg_queue == NULL)
		return -EINVAL;
	struct msg_queue_info* msgqinfo=filep->msg_queue;
	struct msg_queue_member_info members_queue = msgqinfo->members;
	u32 send_pid=msg->from_pid;
	u32 rec_pid=msg->to_pid;
	int send_id=-1,rec_id=-1;
	for(int i=0;i<MAX_MEMBERS;i++){
		if(members_queue.member_pid[i] == send_pid)
			send_id=i;
		if(members_queue.member_pid[i] == rec_pid)
			rec_id=i;
	}
	if(send_id==-1 || ( rec_pid!=BROADCAST_PID && rec_id==-1))
		return -EINVAL;
	/// Send message to all if broadcast message	
	if(rec_pid == BROADCAST_PID){
		int ans=0;
		for(int i=0; i<MAX_MEMBERS; i++){
                if(send_id != i && members_queue.member_pid[i]!=0 && msgqinfo->block_id[send_id][i]!=1){
                    for(int iter=0;iter<=31;iter++){
                        if(msgqinfo->msg_buffer[32*i + iter].from_pid == 0) {
                            msgqinfo->msg_buffer[32*i + iter] = *msg;
    						ans++;
							break;
                        }
                    }
					
                }
				
        }
        return ans;
		
	}	

	//// unicast message
	if(send_id == rec_id)
		return 0;
	if(msgqinfo->block_id[send_id][rec_id]==1)
		return -EINVAL;
		
	for(int iter=0;iter<=31;iter++){
			if(msgqinfo->msg_buffer[32*rec_id + iter].from_pid == 0) {
				msgqinfo->msg_buffer[32*rec_id + iter] = *msg;
				break;
			}
	}
				
	
 	return 1;
}				

void do_add_child_to_msg_queue(struct exec_context *child_ctx)
{
	/** 
	 * TODO Implementation of fork handler 
	 **/
	u32 child_pid=child_ctx->pid;
	//Now traverse all the fd in child_ctx

	for(int i=3;i<MAX_OPEN_FILES;i++){
		if(child_ctx->files[i]){
			if(child_ctx->files[i]->msg_queue){
				struct msg_queue_info* msgqinfo = child_ctx->files[i]->msg_queue;
				msgqinfo->members.member_count = msgqinfo->members.member_count+ 1;
				msgqinfo->members.member_pid[msgqinfo->members.member_count - 1] = child_pid;                
			}
		}
	}
}

void do_msg_queue_cleanup(struct exec_context *ctx)
{
	/** 
	 * TODO Implementation of exit handler 
	 **/
	for(int i=3;i<MAX_OPEN_FILES;i++){
		if(ctx->files[i] && ctx->files[i]->msg_queue)
			do_msg_queue_close(ctx,i);
	}
}

int do_msg_queue_get_member_info(struct exec_context *ctx, struct file *filep, struct msg_queue_member_info *info)
{
	/** 
	 * TODO Implementation of exit handler 
	 **/
	if(filep ==NULL || filep->msg_queue == NULL)
		return -EINVAL;
	struct msg_queue_info* msgqinfo=filep->msg_queue;
	struct msg_queue_member_info members_queue = msgqinfo->members;
	*info =members_queue;
	return 0;
}


int do_get_msg_count(struct exec_context *ctx, struct file *filep)
{
	/** 
	 * TODO Implement functionality to
	 * return pending message count to calling process
	 **/

	/// same as rcv
	if(filep ==NULL || filep->msg_queue == NULL)
		return -EINVAL;
	struct msg_queue_info* msgqinfo=filep->msg_queue;
	struct msg_queue_member_info members_queue = msgqinfo->members;
	int rec_id=-1;
	for(int i=0;i<members_queue.member_count;i++)
		if(members_queue.member_pid[i]==ctx->pid)
			rec_id=i;
	if(rec_id == -1)
		return -EINVAL;

	int ans=0;
	for(int iter=0;iter<=31;iter++){
		if(msgqinfo->msg_buffer[32*rec_id + iter].from_pid)  
			ans++;
	}
	return ans;
}

int do_msg_queue_block(struct exec_context *ctx, struct file *filep, int pid)
{
	/** 
	 * TODO Implement functionality to
	 * block messages from another process 
	 **/
	if(filep ==NULL || filep->msg_queue == NULL)
		return -EINVAL;
	struct msg_queue_info* msgqinfo=filep->msg_queue;
	struct msg_queue_member_info members_queue = msgqinfo->members;
	
	int send_id=-1,rec_id=-1;
	for(int i=0;i<MAX_MEMBERS;i++){
			if(members_queue.member_pid[i] == pid)
				send_id=i;
			if(members_queue.member_pid[i] == ctx->pid)
				rec_id=i;	
	}
	if( send_id==rec_id || send_id==-1 || rec_id==-1 )
		return -EINVAL;
	msgqinfo->block_id[send_id][rec_id]=1;
	return 0;
}

int do_msg_queue_close(struct exec_context *ctx, int fd)
{
	/** 
	 * TODO Implement functionality to
	 * remove the calling process from the message queue 
	 **/
	if(!ctx->files[fd] || !ctx->files[fd]->msg_queue)
		return -EINVAL;
	
	int mypid = ctx->pid;
	struct msg_queue_info* msgqinfo = ctx->files[fd]->msg_queue;
	ctx->files[fd] = NULL;
	
	msgqinfo->members.member_count = msgqinfo->members.member_count - 1;
	int var;
	for(int i=0; i<MAX_MEMBERS; i++) {
		if(msgqinfo->members.member_pid[i] == mypid){ 		
			var = i;
			msgqinfo->members.member_pid[i] = 0;
			break;
		}
	}

	for(int i=0; i<MAX_MEMBERS; i++){
		msgqinfo->block_id[i][var] = 0;
		msgqinfo->block_id[var][i] = 0;
	}

	if( msgqinfo->members.member_count == 0){
		free_msg_queue_buffer(msgqinfo->msg_buffer);
		free_msg_queue_info(msgqinfo);
	}
	return 0;
}
