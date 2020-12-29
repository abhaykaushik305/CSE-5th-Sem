#include<types.h>
#include<context.h>
#include<file.h>
#include<lib.h>
#include<serial.h>
#include<entry.h>
#include<memory.h>
#include<fs.h>
#include<kbd.h>


/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/

void free_file_object(struct file *filep)
{
	if(filep)
	{
		os_page_free(OS_DS_REG ,filep);
		stats->file_objects--;
	}
}

struct file *alloc_file()
{
	struct file *file = (struct file *) os_page_alloc(OS_DS_REG); 
	file->fops = (struct fileops *) (file + sizeof(struct file)); 
	bzero((char *)file->fops, sizeof(struct fileops));
	file->ref_count = 1;
	file->offp = 0;
	stats->file_objects++;
	return file; 
}

void *alloc_memory_buffer()
{
	return os_page_alloc(OS_DS_REG); 
}

void free_memory_buffer(void *ptr)
{
	os_page_free(OS_DS_REG, ptr);
}

/* STDIN,STDOUT and STDERR Handlers */

/* read call corresponding to stdin */

static int do_read_kbd(struct file* filep, char * buff, u32 count)
{
	kbd_read(buff);
	return 1;
}

/* write call corresponding to stdout */

static int do_write_console(struct file* filep, char * buff, u32 count)
{
	struct exec_context *current = get_current_ctx();
	return do_write(current, (u64)buff, (u64)count);
}

long std_close(struct file *filep)
{
	filep->ref_count--;
	if(!filep->ref_count)
		free_file_object(filep);
	return 0;
}
struct file *create_standard_IO(int type)
{
	struct file *filep = alloc_file();
	filep->type = type;
	if(type == STDIN)
		filep->mode = O_READ;
	else
		filep->mode = O_WRITE;
	if(type == STDIN){
		filep->fops->read = do_read_kbd;
	}else{
		filep->fops->write = do_write_console;
	}
	filep->fops->close = std_close;
	return filep;
}

int open_standard_IO(struct exec_context *ctx, int type)
{
	int fd = type;
	struct file *filep = ctx->files[type];
	if(!filep){
		filep = create_standard_IO(type);
	}else{
		filep->ref_count++;
		fd = 3;
		while(ctx->files[fd])
			fd++; 
	}
	ctx->files[fd] = filep;
	return fd;
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/

/* File exit handler */
void do_file_exit(struct exec_context *ctx)
{
	/*TODO the process is exiting. Adjust the refcount
	of files*/
	for(int i=3;i<16;i++){
		if(ctx->files[i]==NULL){
			ctx->files[i]->ref_count -=1;
			if(ctx->files[i]->ref_count ==0)
				free_file_object(ctx->files[i]);
			ctx->files[i]=NULL;
		}
	}
}

/*Regular file handlers to be written as part of the assignmemnt*/


static int do_read_regular(struct file *filep, char * buff, u32 count)
{
	/** 
	*  TODO Implementation of File Read, 
	*  You should be reading the content from File using file system read function call and fill the buf
	*  Validate the permission, file existence, Max length etc
	*  Incase of Error return valid Error code 
	**/
	/*Taken from entry.c */
	int read_size = 0;
	if(!filep){
		return -EINVAL; //file is not opened
	}
	if((filep->mode & O_READ) != O_READ){
		return -EACCES; //file is write only
	}
	read_size=flat_read(filep->inode,buff,(int)count,&(filep->offp));
	if(read_size>0)filep->offp += read_size;
	if(read_size<0)return -EINVAL;
	return read_size;
}

/*write call corresponding to regular file */

static int do_write_regular(struct file *filep, char * buff, u32 count)
{
	/** 
	*   TODO Implementation of File write, 
	*   You should be writing the content from buff to File by using File system write function
	*   Validate the permission, file existence, Max length etc
	*   Incase of Error return valid Error code 
	* */
	int write_size=0;
	if(!filep){
		return -EINVAL; //file is not opened
	}
	if((filep->mode & O_WRITE) != O_WRITE){
		return -EACCES; //file is write only
	}
	write_size=flat_write(filep->inode,buff,(int)count,&(filep->offp));
	if(write_size>0)filep->offp += write_size;
	if(write_size<0)return -EINVAL;
	return write_size;
}

long do_file_close(struct file *filep)
{
	/** TODO Implementation of file close  
	*   Adjust the ref_count, free file object if needed
	*   Incase of Error return valid Error code 
	*/
	if(filep){
		filep->ref_count -=1;
		if(filep->ref_count == 0)
			free_file_object(filep);
		return 0;
	}
	return -EINVAL;
}

static long do_lseek_regular(struct file *filep, long offset, int whence)
{
	/** 
	*   TODO Implementation of lseek 
	*   Set, Adjust the ofset based on the whence
	*   Incase of Error return valid Error code 
	* */
	struct inode *mynode = filep->inode;
	if(whence == SEEK_SET){
		if(offset >= 0 && offset <= mynode->file_size)
		   filep->offp = offset;
        else 
			return -EINVAL;
	}
	else if(whence == SEEK_CUR){

		int temp=filep->offp;
		if(temp + offset >=0 && temp + offset <= mynode->file_size)
			filep->offp = temp + offset;
		else
			return -EINVAL;	
	}
	else if(whence == SEEK_END){
		if(offset<=0)
			filep->offp = mynode->file_size + offset;
		else
			return -EINVAL;
	}
	else return -EINVAL;
	return filep->offp;
	// int ret_fd = -EINVAL; 
	// return ret_fd;
}

extern int do_regular_file_open(struct exec_context *ctx, char* filename, u64 flags, u64 mode)
{

	/**  
	*  TODO Implementation of file open, 
	*  You should be creating file(use the alloc_file function to creat file), 
	*  To create or Get inode use File system function calls, 
	*  Handle mode and flags 
	*  Validate file existence, Max File count is 16, Max Size is 4KB, etc
	*  Incase of Error return valid Error code 
	* */
	struct inode* mynode;
	struct file* fp;
	mynode=lookup_inode(filename);
	if(mynode==NULL){
		if(flags & O_CREAT){
			mynode=create_inode(filename,mode);
			if(mynode==NULL)return -ENOMEM;
			fp=alloc_file();
			fp->mode= flags^(O_CREAT);
			fp->inode=mynode;
			fp->type=REGULAR;
			fp->fops->write=do_write_regular;
			fp->fops->lseek=do_lseek_regular;
			fp->fops->read=do_read_regular;
			fp->fops->close=do_file_close;
			int iter=3;
			while(iter<16 && ctx->files[iter])
				iter++;
			if(iter==16)return -ENOMEM;
			ctx->files[iter]=fp;
			return iter;
		}
		return -EINVAL;
	}
	else{
		if(mynode->mode & flags != flags) 
			return -EACCES;
		fp=alloc_file();
		fp->mode= flags;
		fp->inode=mynode;
		fp->type=REGULAR;
		fp->fops->write=do_write_regular;
		fp->fops->lseek=do_lseek_regular;
		fp->fops->read=do_read_regular;
		fp->fops->close=do_file_close;
		int iter=3;
		while(iter<16 && ctx->files[iter])
			iter++;
		if(iter==16)return -ENOMEM;
		ctx->files[iter]=fp;
		return iter;
		
	}
}

/**
 * Implementation dup 2 system call;
 */
int fd_dup2(struct exec_context *current, int oldfd, int newfd)
{
	/** 
	*  TODO Implementation of the dup2 
	*  Incase of Error return valid Error code 
	**/
	if(!current->files[oldfd])
		return -EINVAL;
	struct file *oldfp=current->files[oldfd];
	struct file *newfp=current->files[newfd];	
	if(!newfp || !newfp->fops || !newfp->fops->close){
                // file is not opened
    }
    else   // If originally closed then fine, else we close it...
        newfp->fops->close(newfp);
	current->files[newfd]=oldfp;
	oldfp->ref_count+=1;
	return newfd;
}

int do_sendfile(struct exec_context *ctx, int outfd, int infd, long *offset, int count) {
	/** 
	*  TODO Implementation of the sendfile 
	*  Incase of Error return valid Error code 
	**/
	if(!ctx->files[infd] || !ctx->files[outfd])
		return -EINVAL;
	struct file* infp=ctx->files[infd];
	struct file* outfp=ctx->files[outfd];
	if( !(infp->mode & O_READ) || !(outfp->mode & O_WRITE))
		return -EACCES;
	char * buff= alloc_memory_buffer();
	if(buff == NULL)
		return -ENOMEM;
	long orig_offset,temp;
	orig_offset=do_lseek_regular(infp,0,SEEK_CUR);
	if(orig_offset == -EINVAL)
		return -EINVAL;
	if(offset !=NULL){
		temp=do_lseek_regular(infp,*offset,SEEK_SET);
		if(temp == -EINVAL)
			return -EINVAL;
	}
	long ret=0,forread,read_size,write_size;
	read_size=do_read_regular(infp,buff,forread);
	if(read_size < 0)
		return read_size;
	for(int i=0;i<read_size;i++){
		write_size=do_write_regular(outfp,&buff[i],1);
		if(write_size!=1)
			break;
		ret+=write_size;
	}
	long temp2=do_lseek_regular(infp,0,SEEK_CUR);
	if(temp2<0)
		return temp2;
	if(offset!=NULL){
		*offset=temp2;
		temp=do_lseek_regular(infp,orig_offset,SEEK_SET);
		if(temp<0)
			return temp;
	}
	else{
		temp=do_lseek_regular(infp,orig_offset+ret,SEEK_SET);
		if(temp<0)
			return temp;
	}
	
	return ret;
}

