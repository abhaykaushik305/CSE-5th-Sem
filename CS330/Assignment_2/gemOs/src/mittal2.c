int do_create_msg_queue(struct exec_context *ctx)
{
        /** 
         * TODO Implement functionality to
         * create a message queue
         **/
        int pid = ctx->pid;
        struct file* filep;

        int ret_fd = -EINVAL;
        for(int i=3; i<MAX_OPEN_FILES;i++){
                if(ctx->files[i] == NULL){
                        // free fd
                        filep = alloc_file();
                        if(!filep) return -ENOMEM;
                        filep->fops->read = NULL;
                        filep->fops->write = NULL;
                        filep->fops->close = NULL;
                        filep->fops->lseek = NULL;
                        struct msg_queue_info *info = alloc_msg_queue_info();
                        if(info == NULL)return -ENOMEM;
                        struct message* buffer = alloc_buffer();
                        if(buffer == NULL)return -ENOMEM;
                        for(int i=0;i<128;i++)buffer[i].from_pid = -1;

                        info->msg_buffer = buffer;
                        info->members.member_count = 1;
                        info->members.member_pid[0] = pid;
                        for(int j=1; j<MAX_MEMBERS;j++){
                                info->members.member_pid[j] = -1;       // pid lies from 0 to 8;
                        }
                        filep->msg_queue = info;
                        ctx->files[i] = filep;
                        ret_fd = i;
                        break;
                }
        }

        return ret_fd;
}

int do_msg_queue_rcv(struct exec_context *ctx, struct file *filep, struct message *msg)
{
        /** 
         * TODO Implement functionality to
         * recieve a message
         **/
        int ret_val = 0;

        int pid = ctx->pid;
        if(!filep || !filep->msg_queue)return -EINVAL;

        struct msg_queue_info* msg_queue = filep->msg_queue;
        struct msg_queue_member_info members = msg_queue->members;
        int pos = 0;
        int flag = 0;
        int mem_count = members.member_count;

        for(int i=0;i<mem_count;i++){
                if(members.member_pid[i] == pid){
                        pos = i;
                        flag = 1;
                }
        }
        if(!flag)return -EINVAL;

        if(msg_queue->msg_buffer[32*pos].from_pid != -1){
                struct message Msg = msg_queue->msg_buffer[32*pos];
                *msg = Msg;

                for(int j=0;j<31;j++){
                        msg_queue->msg_buffer[32*pos + j] = msg_queue->msg_buffer[32*pos + j + 1];
                }
                msg_queue->msg_buffer[32*pos + 31].from_pid = -1;

                ret_val = 1;
        }
        else ret_val = 0;

        return ret_val;
}

int do_msg_queue_send(struct exec_context *ctx, struct file *filep, struct message *msg)
{
        /** 
         * TODO Implement functionality to
         * send a message
         **/
        int pid = ctx->pid;
        if(!filep || !filep->msg_queue)return -EINVAL;

        struct msg_queue_info* msg_queue = filep->msg_queue;
        struct msg_queue_member_info members = msg_queue->members;

        int mem_count = members.member_count;
        int to_pid = msg->to_pid;
        int from_pid = msg->from_pid;
        int pos_to = BROADCAST_PID;
        int pos_from = -1;
        int flag_to = 0;
        int flag_from = 0;


        for(int i=0;i<MAX_MEMBERS;i++){
                if(members.member_pid[i] == from_pid){
                        flag_from = 1;
                        pos_from = i;
                }
                if(members.member_pid[i] == to_pid){
                        flag_to = 1;
                        pos_to = i;
                }
        }
        if(to_pid == BROADCAST_PID)flag_to = 1;
        if(!flag_to || !flag_from)return -EINVAL;

        if(to_pid == BROADCAST_PID){
                for(int i=0; i<mem_count; i++){
                        if(pos_from != i){
                                for(int j=0;j<32;j++){
                                        if(msg_queue->msg_buffer[32*i + j].from_pid == -1) {
                                                msg_queue->msg_buffer[32*i + j] = *msg;
                                                break;
                                        }
                                }
                        }
                }
                return mem_count - 1;
        }
        else{
                for(int i=0; i<mem_count; i++){
                        if(pos_from!=i && pos_to == i){
                                for(int j=0;j<32;j++){
                                        if(msg_queue->msg_buffer[32*i + j].from_pid == -1) {
                                                msg_queue->msg_buffer[32*i + j] = *msg;
                                                break;
                                        }
                                }
                        }
                }
                if(pos_from != pos_to)return 1;
                else return 0;
        }
}

void do_add_child_to_msg_queue(struct exec_context *child_ctx)
{
        /** 
         * TODO Implementation of fork handler 
         **/
        int pid = child_ctx->pid;

        for(int i=3; i<MAX_OPEN_FILES;i++){
                if(child_ctx->files[i] && child_ctx->files[i]->msg_queue){
                        struct msg_queue_info* msg_queue = child_ctx->files[i]->msg_queue;
                        msg_queue->members.member_pid[msg_queue->members.member_count] = pid;
                        msg_queue->members.member_count += 1;
                }
        }

}

void do_msg_queue_cleanup(struct exec_context *ctx)
{
        /** 
         * TODO Implementation of exit handler 
         **/
}

int do_msg_queue_get_member_info(struct exec_context *ctx, struct file *filep, struct msg_queue_member_info *info)
{
        /** 
         * TODO Implementation of exit handler 
         **/
        int pid = ctx->pid;
        if(!filep || !filep->msg_queue)return -EINVAL;

        struct msg_queue_info* msg_queue = filep->msg_queue;
        struct msg_queue_member_info members = msg_queue->members;

        *info = members;

        return 0;
}


int do_get_msg_count(struct exec_context *ctx, struct file *filep)
{
        /** 
         * TODO Implement functionality to
         * return pending message count to calling process
         **/
        int pid = ctx->pid;

        if(!filep || !filep->msg_queue)return -EINVAL;

        struct msg_queue_info* msg_queue = filep->msg_queue;
        struct msg_queue_member_info members = msg_queue->members;

        int pos;
        int flag = 0;
        int mem_count = members.member_count;
        for(int i=0;i<mem_count; i++){
                if(members.member_pid[i] == pid){
                        pos = i;
                        flag = 1;
                }
        }
        if(!flag) return -EINVAL;

        int count = 0;
        for(int j=0;j<32;j++){
                if(msg_queue->msg_buffer[32*pos + j].from_pid != -1)count++;
        }

        return count;
}