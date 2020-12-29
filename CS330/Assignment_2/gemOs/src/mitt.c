/* File exit handler */
void do_file_exit(struct exec_context *ctx)
{       
        /*TODO the process is exiting. Adjust the refcount
        of files*/
        for(int i=3;i<MAX_OPEN_FILES; i++){
                if(ctx->files[i]){
                        // File is open 
                        struct file* fp = ctx->files[i];
                        ctx->files[i] = NULL;
                        do_file_close(fp);
                }
        }
}

/Regular file handlers to be written as part of the assignmemnt/


static int do_read_regular(struct file *filep, char * buff, u32 count)
{
        /** 
        *  TODO Implementation of File Read, 
        *  You should be reading the content from File using file system read function call and fill the buf
        *  Validate the permission, file existence, Max length etc
        *  Incase of Error return valid Error code 
        **/
        struct inode* Inode = filep->inode;
        int read_size = flat_read(Inode, buff, (int)count, &(filep->offp));
        if(read_size > 0) filep->offp += read_size;

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
        struct inode* Inode = filep->inode;
        int write_size = flat_write(Inode, buff, (int)count, &(filep->offp));
        if(write_size > 0) filep->offp +=  write_size;

        return write_size;
}


long do_file_close(struct file *filep)
{
        /** TODO Implementation of file close  
        *   Adjust the ref_count, free file object if needed
        *   Incase of Error return valid Error code 
        */
        if(!filep){
                printk("Can't be closed, file pointer invalid");
                return -EINVAL;
        }
        filep->ref_count -= 1;
        if(filep->ref_count == 0){
                free_file_object(filep);
        }
        return 0;
}

static long do_lseek_regular(struct file *filep, long offset, int whence)
{
        /** 
        *   TODO Implementation of lseek 
        *   Set, Adjust the ofset based on the whence
        *   Incase of Error return valid Error code 
        * */
        if(whence == SEEK_SET){
                if(offset >= 0 && offset < file_size)   filep->offp = offset;
                else return -EINVAL;
        }
        if(whence == SEEK_END){
                if(SEEK_END + offset >= 0 && SEEK_END + offset < file_size)     filep->offp = SEEK_END + offset;
                else return -EINVAL;
        }
        if(whence == SEEK_CUR){
                int curr = filep->offp;
                if(curr + offset >= 0 && curr + offset < file_size)     filep->offp = curr + offset;
                else return -EINVAL;
        }

        return filep->offp;
}

extern int do_regular_file_open(struct exec_context ctx, char filename, u64 flags, u64 mode)
{

        /**  
        *  TODO Implementation of file open, 
        *  You should be creating file(use the alloc_file function to creat file), 
        *  To create or Get inode use File system function calls, 
        *  andle mode and flags 
        *  Validate file existence, Max File count is 16, Max Size is 4KB, etc
        *  Incase of Error return valid Error code 
        * */
        struct inode* Inode = lookup_inode(filename);
        struct file* file_p;
        int ret_fd = -EINVAL;


        if( (flags >> 3) == 1 &&  Inode == NULL){
        // File doesn't exist , so create a file...
        // Create file with given mode ...
                Inode = create_inode(filename, mode);
        }

        if(Inode == NULL){
                return -ENOMEM;
        }

        int temp1 = Inode->mode;
        int temp2 = flags;

        for(int i=0;i<3;i++){
                if( !((temp1 >> i)%2)&((temp2 >> i)%2)){
                       return -EACCES;
                }
        }


        for(int i=3;i<MAX_OPEN_FILES; i++){
                if(ctx->files[i] == NULL){
                        file_p = alloc_file();
                        file_p->inode = Inode;
                        if( (flags & O_CREAT) != O_CREAT){
                                file_p->mode = flags;
                        }
                        else{
                                file_p->mode = flags^O_CREAT;
                        }
                        file_p->fops->read = &(do_read_regular);
                        file_p->fops->write = &(do_write_regular);
                        file_p->fops->close = &(do_file_close);
                        file_p->fops->lseek = &(do_lseek_regular);
                        ctx->files[i] = file_p;
                        ret_fd = i;
                        break;
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

        struct file *old_fp = current->files[oldfd];
        if(!old_fp || !old_fp->fops || !old_fp->fops->close){
                printk("Old fd not opened");
                return -EINVAL; //file is not opened
        }

        struct file *new_fp = current->files[newfd];
        if(!new_fp || !new_fp->fops || !new_fp->fops->close){
                //new fd file is not opened
        }
        else{
                current->files[newfd] = NULL;   // If originally closed then fine, else we close it...
                new_fp->fops->close(new_fp);
        }
        old_fp->ref_count += 1;
        current->files[newfd] = current->files[oldfd];  //duplicate oldfd into newfd

        return newfd;
}