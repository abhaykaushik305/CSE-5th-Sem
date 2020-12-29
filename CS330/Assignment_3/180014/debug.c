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
	ctx->state= WAITING;
	struct exec_context* par = get_ctx_by_pid(ctx->ppid);
	
	if(par==NULL || par->dbg == NULL)
		return -1;
	par->state = READY;
	par->regs.rax=ctx->regs.entry_rip -1;
	
	par->dbg->backarr[0] = ctx->regs.entry_rip -1; 
	

	ctx->regs.entry_rsp -= 0x8;
	u64 *temp = (u64 *)ctx->regs.entry_rsp;
	*temp = ctx->regs.rbp;

	int idx=1;

	u64 *tmp = (u64 *)ctx->regs.entry_rsp;
	while(*(tmp+1) != END_ADDR && idx<MAX_BACKTRACE){
		par->dbg->backarr[idx] = *(tmp+1);
		tmp = (u64 *) *tmp;
		idx++;
	}
	par->dbg->bt_count = idx;
	ctx->regs.rax=0;
	schedule(par);
	return 0;
}

/*
 * Exit handler.
 * Called on exit of Debugger and Debuggee 
 */
void debugger_on_exit(struct exec_context *ctx)
{

	// Your code	
	if(ctx==NULL)
		return ;
	if(ctx->dbg == NULL){
		struct exec_context* par = get_ctx_by_pid(ctx->ppid);
		par->regs.rax = CHILD_EXIT;
		par->state = READY;
		ctx->state = WAITING;
	}
	else{
		struct breakpoint_info *head = ctx->dbg->head;
		while(head)
		{
			struct breakpoint_info *temp = head->next;
			free_breakpoint_info(head);
			head = temp;
		}
		free_debug_info(ctx->dbg);
	}
	return;
}

/*
 * called from debuggers context
 * initializes debugger state
 */
int do_become_debugger(struct exec_context *ctx)
{
	// Your code
	struct debug_info* ctx_info = alloc_debug_info();
	if(ctx_info == NULL)
			return -1;
	ctx_info->total_BP=0;
	ctx_info->num_BP=0;
	ctx_info->head=NULL;
	ctx_info->bt_count = 0;
	ctx->dbg=ctx_info;
	for(int i=0;i<MAX_BACKTRACE;i++)
		ctx->dbg->backarr[i] = 0;
	return 0;
}

/*
 * called from debuggers context
 */
int do_set_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code
	if(ctx == NULL || ctx->dbg == NULL)
		return -1;
	struct breakpoint_info *head= ctx->dbg->head;
	struct breakpoint_info *temp = alloc_breakpoint_info();
	


	if(head == NULL){
		if(temp==NULL)
			return -1;
		temp->num = ctx->dbg->total_BP + 1;
		temp->next= NULL;
		temp->status = 1; //enabled
		temp->addr = (u64)addr;
		*((u8 *)addr) = 0xCC;
		ctx->dbg->head = temp;
		ctx->dbg->num_BP++;
		ctx->dbg->total_BP++;
		return 0;
	}

	while(head->next){
		if(head->addr == (u64) addr){
			head->status = 1;
			
			*((u8 *)addr) = 0xCC; //INT3_OPCODE
			return 0;
		}
		head=head->next;
	}
	
	if(temp == NULL)
		return -1;
	if(ctx->dbg->total_BP == MAX_BREAKPOINTS )
		return -1;
	

	temp->num = ctx->dbg->total_BP + 1;
	temp->next = NULL;
	temp->status = 1; //enabled
	temp->addr = (u64)addr;

	head->next = temp;
	ctx->dbg->num_BP++;
	ctx->dbg->total_BP++;
	*((u8 *)addr) = 0xCC;
	return 0;
		
}

/*
 * called from debuggers context
 */
int do_remove_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code
	if(ctx==NULL || ctx->dbg == NULL)
		return -1;
	struct breakpoint_info* head = ctx->dbg->head;
	if(head == NULL)
		return -1;

	if(head->addr == (u64)addr){
		ctx->dbg->head = head->next;
		free_breakpoint_info(head);
		ctx->dbg->num_BP--;
		*((u8 *)addr) = 0x55;
		return 0;
	}

	while(head->next){
		if(head->next->addr== (u64)addr){
			struct breakpoint_info* temp = head->next;
			head->next =  temp->next;
			*((u8 *)addr) = 0x55; //  PUSHRBP_OPCODE
			free_breakpoint_info(temp);			
			ctx->dbg->num_BP--;
			return 0;
		}
		head=head->next;
	}
	return -1;
}

/*
 * called from debuggers context
 */
int do_enable_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code
	if(ctx == NULL || ctx->dbg == NULL)
		return -1;
	struct breakpoint_info* head = ctx->dbg->head;

	while(head!=NULL){
		if(head->addr == (u64)addr){
			*((u8 *)addr) = 0xCC;
			head->status = 1;
			return 0;
		}
		head=head->next;
	}
	return -1;
}

/*
 * called from debuggers context
 */
int do_disable_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code
	if(ctx == NULL || ctx->dbg == NULL)
		return -1;	
	struct breakpoint_info* head = ctx->dbg->head;

	while(head!=NULL){
		if(head->addr == (u64)addr){
			*((u8 *)addr) = 0x55;
			head->status = 0;
			return 0;
		}
		head=head->next;
	}
	return -1;
}

/*
 * called from debuggers context
 */ 
int do_info_breakpoints(struct exec_context *ctx, struct breakpoint *ubp)
{
	// Your code
	if(ctx == NULL || ctx->dbg == NULL || ubp == NULL)
		return -1;
	struct breakpoint_info* head = ctx->dbg->head;
	int idx=0;

	while(head!=NULL){
		ubp[idx].num = head->num;
		ubp[idx].addr = head->addr;
		ubp[idx].status = head->status;
		head=head->next;
		idx++;
	}
	return idx;
}

/*
 * called from debuggers context
 */
int do_info_registers(struct exec_context *ctx, struct registers *regs)
{
	// Your code
	if(ctx == NULL || ctx->dbg == NULL)
		return -1;
		
	int found=0;
	struct exec_context *child;
	
	for(int i=1; i<MAX_PROCESSES; i++){
        child = get_ctx_by_pid(i);
        if(child->ppid == ctx->pid){
            found = 1;
            break;
        }
    }

    if(found==0)
    	return -1;

	regs->entry_rip = child->regs.entry_rip - 0x1; 
	regs->entry_rsp = child->regs.entry_rsp + 0x8;
	regs->rbp = child->regs.rbp;
	regs->rax = child->regs.rax;
	

	regs->rdi = child->regs.rdi;
	regs->rsi = child->regs.rsi;
	regs->rdx = child->regs.rdx;
	regs->rcx = child->regs.rcx;
	regs->r9  = child->regs.r9;
	regs->r8  = child->regs.r8;
	
	return 0;
}

/* 
 * Called from debuggers context
 */
int do_backtrace(struct exec_context *ctx, u64 bt_buf)
{
	// Your code
	if(ctx==NULL || ctx->dbg == NULL )
		return -1;
	for(int i=0; i< ctx->dbg->bt_count; i++){
		*((u64*)bt_buf + i) = ctx->dbg->backarr[i];
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
	ctx->state = WAITING;
	
	int found = 0;
	struct exec_context *child;

	for(int i = 1; i<MAX_PROCESSES; i++){		//Assumption : Only 2 processes are running
			
		child = get_ctx_by_pid(i);
		if(child && child->ppid == ctx->pid){
			child->state = READY;	
			found = 1;
			break;
		}
	}
	if(found == 0)
		return -1;
 	
	schedule(child);
	
	return 0;
}

