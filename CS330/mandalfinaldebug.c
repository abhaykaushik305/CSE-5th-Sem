#include <debug.h>
#include <context.h>
#include <entry.h>
#include <lib.h>
#include <memory.h>


/*****************************HELPERS******************************************/

/* 
 * allocate the struct which contains information about debugger
 *
 */
struct debug_info *alloc_debug_info()
{
	struct debug_info *info = (struct debug_info *) os_alloc(sizeof(struct debug_info)); 
	if(info)
		bzero((char *)info, sizeof(struct debug_info));
	return info;
}

/*
 * frees a debug_info struct 
 */
void free_debug_info(struct debug_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct debug_info));
}

/*
 * allocates memory to store registers structure
 */
struct registers *alloc_regs()
{
	struct registers *info = (struct registers*) os_alloc(sizeof(struct registers)); 
	if(info)
		bzero((char *)info, sizeof(struct registers));
	return info;
}

/*
 * frees an allocated registers struct
 */
void free_regs(struct registers *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct registers));
}

/* 
 * allocate a node for breakpoint list 
 * which contains information about breakpoint
 */
struct breakpoint_info *alloc_breakpoint_info()
{
	struct breakpoint_info *info = (struct breakpoint_info *)os_alloc(
		sizeof(struct breakpoint_info));
	if(info)
		bzero((char *)info, sizeof(struct breakpoint_info));
	return info;
}

/*
 * frees a node of breakpoint list
 */
void free_breakpoint_info(struct breakpoint_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct breakpoint_info));
}

/*
 * Fork handler.
 * The child context doesnt need the debug info
 * Set it to NULL
 * The child must go to sleep( ie move to WAIT state)
 * It will be made ready when the debugger calls wait_and_continue
 */
void debugger_on_fork(struct exec_context *child_ctx)
{
	child_ctx->dbg = NULL;	
	child_ctx->state = WAITING;	
}


/******************************************************************************/

/* This is the int 0x3 handler
 * Hit from the childs context
 */
long int3_handler(struct exec_context *ctx)
{
	// Your code
	struct exec_context *parent = get_ctx_by_pid(ctx->ppid);
	ctx->state = WAITING;
	if(parent == NULL) return -1;//parent not found due to some reason
	parent->state = READY;

	parent->regs.rax = ctx->regs.entry_rip-1;//RIP register gives next instruction's address so decrement it by 1 to get breakpoint's address
//	*((u8 *)ctx->regs.entry_rip) = 0x55;//typecast the address to long type pointer,  dereference it to store the pushrbp operation code

	//Manually implement PUSHRBP_OPCODE by pushing the rbp register to the top of the stack
	ctx->regs.entry_rsp -= 8;// allocate 8 byte for the rbp since it stores an address
	*((u64 *)ctx->regs.entry_rsp) = ctx->regs.rbp;
	
	int i = 1;
	u64 *bt = parent->dbg->bt_buf;
	bt[0] = ctx->regs.entry_rip-1;
	u64 *itr = (u64 *)ctx->regs.entry_rsp;
	while(*(itr+1) != END_ADDR && i<MAX_BACKTRACE){
		bt[i] = *(itr+1);
		itr = (u64 *) *itr;
		//printk("bt at %d : %x\n", i, bt[i]);
		i++;
	}
	parent->dbg->bt_buf;
	parent->dbg->bt_count = i;
//	printk("child backtrace array:\n");
//	for(int j=0;j<i;j++){
//		ctx->dbg->bt_buf[j] = bt[j];
//		printk("%d : child:%x, bt:%x\n", j, ctx->dbg->bt_buf[j], bt[j]);
//	}

	ctx->regs.rax = 0;//succesful implement of int3
	schedule(parent);//schedule the debuggee if interrupt handler is called
}

/*
 * Exit handler.
 * Called on exit of Debugger and Debuggee 
 */
void debugger_on_exit(struct exec_context *ctx)
{
	// Your code	
	if(ctx->dbg)//process is a debugger
	{
		struct breakpoint_info *curr = ctx->dbg->head;
		while(curr != NULL)//free each and every breakpoint till list ends
		{
			struct breakpoint_info *temp = curr->next;//store the next pointer before freeing current pointer
			free_breakpoint_info(curr);
			curr = temp;
		}
		free_debug_info(ctx->dbg);
	}
	else {// if the process is a debuggee
		struct exec_context *parent = get_ctx_by_pid(ctx->ppid);
	        ctx->state = WAITING;
        	parent->state = READY;
		parent->regs.rax = CHILD_EXIT;//notify the debugger that child has exited
	}
}

/*
 * called from debuggers context
 * initializes debugger state
 */
int do_become_debugger(struct exec_context *ctx)
{
	// Your code
	ctx->dbg = alloc_debug_info();
	if(ctx->dbg){//succesfully allocated memory
		ctx->dbg->num_bp = 0;
		ctx->dbg->bp_count = 0;
		ctx->dbg->bt_buf = (u64 *) os_alloc(MAX_BACKTRACE*8);
		for(int i=0;i<MAX_BACKTRACE;i++)
			ctx->dbg->bt_buf[i] = 0;
		return 0;
	}
	return -1;
}

/*
 * called from debuggers context
 */
int do_set_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code
	if(ctx->dbg->num_bp == MAX_BREAKPOINTS)//maximum number of breakpoints reached
		return -1;

	if(ctx->dbg == NULL)
		do_become_debugger(ctx);

	struct breakpoint_info *new_bp = alloc_breakpoint_info();

	struct breakpoint_info *temp = ctx->dbg->head;

	new_bp->num = ctx->dbg->bp_count+1;
	new_bp->status = 1;
	new_bp->addr = (u64)addr;
	new_bp->next = NULL;

	if(temp == NULL) {
		*((u8 *)addr) = 0xCC;
		ctx->dbg->head = new_bp;
		ctx->dbg->num_bp++;
		ctx->dbg->bp_count++;
		return 0;
	}

	while(temp != NULL){
		if(temp->addr == (u64)addr){
			*((u8 *)addr) = 0xCC;
			temp->status = 1;//if the breakpoint already exist enable it
			return 0;
		}
		temp = temp->next;
	}

	*((u8 *)addr) = 0xCC;
	temp->next = new_bp;
	ctx->dbg->num_bp++;
	ctx->dbg->bp_count++;

	return 0;
}

/*
 * called from debuggers context
 */
int do_remove_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code	
	struct breakpoint_info *prev = ctx->dbg->head;
	struct breakpoint_info *curr = prev->next;
	*((u8 *)addr) = 0x55;
	if(prev->addr == (u64)addr){//if the head corresponds to breakpoint address
		*((u8 *)addr) = 0x55;
		ctx->dbg->head = curr;
		free_breakpoint_info(prev);
		ctx->dbg->num_bp--;
		return 0;
	}

	while(curr->next != NULL){
		if(curr->addr == (u64)addr){//found the node corresponding to address
			*((u8 *)addr) = 0x55;
	                prev->next = curr->next;
        	        free_breakpoint_info(curr);
                	ctx->dbg->num_bp--;
                	return 0;
		}
		curr = curr->next;
		prev = prev->next;
	}
	return -1;//breakpoint not found
}

/*
 * called from debuggers context
 */
int do_enable_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code
	struct breakpoint_info *curr = ctx->dbg->head;
	while(curr != NULL){
		if(curr->addr == (u64)addr){//enable breakpoint by adding int3 code and set status to enable
			*((u8 *)addr) = 0xCC;
			curr->status = 1;
			return 0;
		}
		curr = curr->next;
	}
	return -1;//breakpoint not found
}

/*
 * called from debuggers context
 */
int do_disable_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code
	struct breakpoint_info *curr = ctx->dbg->head;
	while(curr != NULL){
		if(curr->addr == (u64)addr){//disable breakpoint by removing int3 code and set status to disable
			*((u8 *)addr) = 0x55;
			curr->status = 0;
			return 0;
		}
		curr = curr->next;
	}
	return -1;//breakpoint not found
}

/*
 * called from debuggers context
 */ 
int do_info_breakpoints(struct exec_context *ctx, struct breakpoint *ubp)
{
	// Your code
	if(ctx->dbg == NULL) return -1;//if the process is not a debugger
	int i = 0;
	struct breakpoint_info *curr = ctx->dbg->head;
	while(curr != NULL){
		ubp[i].num = curr->num;
		ubp[i].status = curr->status;
		ubp[i].addr = curr->addr;
		i++;
		curr = curr->next;
	}
	return i;
}

/*
 * called from debuggers context
 */
int do_info_registers(struct exec_context *ctx, struct registers *regs)
{
	// Your code
	if(ctx->dbg == NULL)return -1;//if process is not a debugger
	struct exec_context *child = NULL;
	// find the child process since parent is called just after breakpoint is encountered the registers in the child process remain as it is
        for(int i=1; i<MAX_PROCESSES; i++){
                struct exec_context *temp = get_ctx_by_pid(i);
                if(temp->ppid == ctx->pid){
                        child = temp;
                }
        }
        if(child == NULL)return -1;	//if child not found
	struct user_regs ch_reg = child->regs;
	regs->r11 = ch_reg.r11;
	regs->r10 = ch_reg.r10;
	regs->r9 = ch_reg.r9;
	regs->r8 = ch_reg.r8;
	regs->rbp = ch_reg.rbp;
	regs->rdi = ch_reg.rdi;
	regs->rsi = ch_reg.rsi;
	regs->rdx = ch_reg.rdx;
	regs->rcx = ch_reg.rcx;
	regs->rbx = ch_reg.rbx;
	regs->rax = ch_reg.rax;
	regs->entry_rip = ch_reg.entry_rip-1;
	regs->entry_cs = ch_reg.entry_cs;
	regs->entry_rflags = ch_reg.entry_rflags;
	regs->entry_rsp = ch_reg.entry_rsp;
	regs->entry_ss = ch_reg.entry_ss;
	return 0;
}

/* 
 * Called from debuggers context
 */
int do_backtrace(struct exec_context *ctx, u64 bt_buf)
{
	// Your code
	u64 *buf = (u64 *)bt_buf;
	for(int i=0; i<ctx->dbg->bt_count; i++){
		buf[i] = ctx->dbg->bt_buf[i];
//		printk("bt at %d : %x\n", i, buf[i]);
	}
	return ctx->dbg->bt_count;
}


/*
 * When the debugger calls wait
 * it must move to WAITING state 
 * and its child must move to READY state
 */

s64 do_wait_and_continue(struct exec_context *ctx)
{
	// Your code	
	ctx->state = WAITING;//change state of debugger(parent) to waiting
	struct exec_context *child;
	int flag = 0;//assume child(debugee) not found
	for(int i=1; i<MAX_PROCESSES; i++){
		struct exec_context *temp = get_ctx_by_pid(i);
		if(temp->ppid == ctx->pid){
			child = temp;
			flag = 1;//child process found
		}
	}
	if(flag == 0)return -1;
	child->state = READY;//change debugee to ready state
	schedule(child);//schedule the child
}

