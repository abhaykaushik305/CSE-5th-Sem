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
//	printk("INT3 Handler called\n");
	ctx->state = WAITING;
	struct exec_context *debugger_ctx = get_ctx_by_pid(ctx->ppid);
	if(!debugger_ctx || debugger_ctx->dbg == NULL){
		return -1;
	}
	
//	printk("rbp value : %x\n", ctx->regs.rbp);
//	printk("entry_rsp before : %x\n", ctx->regs.entry_rsp);
//	printk("data at rsp : %x\n", *(u64*)ctx->regs.entry_rsp);	

	/* Manually Push rbp  into user stack */
	// push rbp : This instruction is used to correctly return to the main function after calling 
//	printk("brkpt hit at : %x\n", ctx->regs.entry_rip - 1);

	debugger_ctx->dbg->backtrace[0] = ctx->regs.entry_rip - 1;
	debugger_ctx->dbg->backtrace[1] = *(u64*)ctx->regs.entry_rsp;	

	ctx->regs.entry_rsp -= 0x8;
	u64* data = (u64*)ctx->regs.entry_rsp;
	*data = ctx->regs.rbp;

	u64 ret_addr = *(u64*)(*data + 8), rbp_val = *(u64*)ctx->regs.rbp;

	int idx = 2;
	while( ret_addr != END_ADDR ){
		debugger_ctx->dbg->backtrace[idx++] = ret_addr;	
		ret_addr = *(u64*)(rbp_val + 8);
		rbp_val = *(u64*)rbp_val;	
	}
	debugger_ctx->dbg->bt_count = idx;

//	printk("entry_rip : %x\n", ctx->regs.entry_rip);
//	printk("entry_rsp after: %x\n", ctx->regs.entry_rsp);
	
	debugger_ctx->regs.rax = ctx->regs.entry_rip - 1;	// We need to return the breakpoint addr.
	debugger_ctx->state = READY;				// Debugger is READY!
		
	schedule(debugger_ctx);					// Schedule it

	return 0;
}

/*
 * Exit handler.
 * Called on exit of Debugger and Debuggee 
 */
void debugger_on_exit(struct exec_context *ctx)
{
	// Your code	
	if(ctx->dbg == NULL){
	    //Child
		struct exec_context* debugger_ctx = get_ctx_by_pid(ctx->ppid);
		debugger_ctx->regs.rax = CHILD_EXIT;
		debugger_ctx->state = READY;
	}
	else{
	    //Debugger 
		struct breakpoint_info* temp, *curr = ctx->dbg->head;
		while(curr != NULL){
			temp = curr;
			curr = curr->next;
			free_breakpoint_info(temp);
		}
		free_debug_info(ctx->dbg);
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
	if(!ctx->dbg){
		// Exit the current running process(debugger)
		return -1;
	}
	ctx->dbg->head = NULL;
	ctx->dbg->num_breakpoints = 0; 
	ctx->dbg->prev_num = 0;
	ctx->dbg->bt_count = 0;
	return 0;
}

/*
 * called from debuggers context
 */
int do_set_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code
	struct breakpoint_info* prev, *curr = ctx->dbg->head;
	struct breakpoint_info* temp = alloc_breakpoint_info();
	if(curr == NULL){
		if(!temp)return -1;
		ctx->dbg->num_breakpoints = 1;
		ctx->dbg->prev_num += 1;
		temp->num = ctx->dbg->prev_num;
		temp->status = 1;
		temp->addr = (u64)addr;
		u64* data = (u64*)addr;
		*data = (*data & ~0xFF) | INT3_OPCODE;
		temp->next = NULL;
		ctx->dbg->head = temp;	
	}
	else{
		while(curr != NULL){
			if(curr->addr == (u64)addr){
				curr->status = 1;
//				printk("Brkpt on address already exists\n");
				return 0;
			}
			prev = curr;
			curr = curr->next;		
		}
		if(ctx->dbg->num_breakpoints == MAX_BREAKPOINTS){
//			printk("Num breakpoints exceed Max limit\n");
			return -1;
		}
		if(!temp) return -1;
		ctx->dbg->num_breakpoints += 1;
		ctx->dbg->prev_num += 1;
		temp->num = ctx->dbg->prev_num;
		temp->addr = (u64)addr;
		u64* data = (u64*)addr;
                *data = (*data & ~0xFF) | INT3_OPCODE;
		temp->status = 1;
		temp->next = NULL;
		prev->next = temp;
		//printk("Num breakpoints are %d\n", temp->num);
	}
	return 0;
}

/*
 * called from debuggers context
 */
int do_remove_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code
	struct breakpoint_info* prev = NULL, *curr = ctx->dbg->head;
	if(curr == NULL){
//		printk("Breakpoint List is Empty\n");
		return -1;
	}	
	else{
		while(curr != NULL){
			if(curr->addr == (u64)addr){
//				printk("Address found in List\n");
				if(prev == NULL)ctx->dbg->head = curr->next;	// Deleted curr;
				else{
					prev->next = curr->next;
				}
				ctx->dbg->num_breakpoints -= 1;
				// Change the data at addr;
				u64* data = (u64 *)((u64)addr);
				*data = (*data & ~0xFF) | PUSHRBP_OPCODE;
		
//				printk("New data : %x\n", *data);
				
				return 0;
			}
			prev = curr;
			curr = curr->next;
		}
	}	
	return -1;
}

/*
 * called from debuggers context
 */
int do_enable_breakpoint(struct exec_context *ctx, void *addr)
{
	if(!ctx || !ctx->dbg) return -1;

	struct breakpoint_info* curr = ctx->dbg->head;
	while(curr != NULL){
		if(curr->addr == (u64)addr){
			curr->status = 1;
			u64* data = (u64*)addr;
                	*data = (*data & ~0xFF) | INT3_OPCODE;
			return 0;
		}
		curr = curr->next;
	}
	
	return -1;
}

/*
 * called from debuggers context
 */
int do_disable_breakpoint(struct exec_context *ctx, void *addr)
{
	if(!ctx || !ctx->dbg) return -1;

        struct breakpoint_info* curr = ctx->dbg->head;
        while(curr != NULL){
                if(curr->addr == (u64)addr){
                        curr->status = 0;
                        u64* data = (u64*)addr;
                        *data = (*data & ~0xFF) | PUSHRBP_OPCODE;
                        return 0;
                }
		curr = curr->next;
        }

        return -1;	
}

/*
 * called from debuggers context
 */ 
int do_info_breakpoints(struct exec_context *ctx, struct breakpoint *ubp)
{
	if(!ctx || !ctx->dbg)return -1; 	// Given ctx is wrong 

	struct breakpoint_info* curr = ctx->dbg->head;
	int idx = 0;
	if(ubp == NULL)return -1;		// No memory allocated to ubp

	while(curr != NULL){
		ubp[idx].num = curr->num;
		ubp[idx].status = curr->status;
		ubp[idx].addr = curr->addr;
		idx++;
		curr = curr->next;
	}
	return ctx->dbg->num_breakpoints;
}

/*
 * called from debuggers context
 */
int do_info_registers(struct exec_context *ctx, struct registers *regs)
{
	// Your code
	struct exec_context* cctx;
	int found = 0;
	for(int p = 1; p < MAX_PROCESSES; p++){
		cctx = get_ctx_by_pid(p);
		if(cctx->ppid == ctx->pid){
			found = 1;
			break;
		}
	}	
	if(!found) return -1;

	regs->r15 = cctx->regs.r15;
	regs->r14 = cctx->regs.r14;
	regs->r13 = cctx->regs.r13;
	regs->r12 = cctx->regs.r12;
	regs->r11 = cctx->regs.r11;
	regs->r10 = cctx->regs.r10;
	regs->r9  = cctx->regs.r9;
	regs->r8  = cctx->regs.r8;
	regs->rbp = cctx->regs.rbp;
	regs->rdi = cctx->regs.rdi;
	regs->rsi = cctx->regs.rsi;
	regs->rdx = cctx->regs.rdx;
	regs->rcx = cctx->regs.rcx;
	regs->rbx = cctx->regs.rbx;
	regs->rax = cctx->regs.rax;
	regs->entry_rip = cctx->regs.entry_rip - 1;  
	regs->entry_cs = cctx->regs.entry_cs;  
	regs->entry_rflags = cctx->regs.entry_rflags;
	regs->entry_rsp = cctx->regs.entry_rsp + 0x8;
	regs->entry_ss = cctx->regs.entry_ss;
	

	return 0;
}

/* 
 * Called from debuggers context
 */
int do_backtrace(struct exec_context *ctx, u64 bt_buf)
{
	// Your code

	for(int i=0; i< ctx->dbg->bt_count; i++){
		*((u64*)bt_buf + i) = ctx->dbg->backtrace[i];
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
	u32 pid = ctx->pid;
	int found = 0;
	struct exec_context *child_ctx;

	for(int p = 1; p < MAX_PROCESSES; p++){		//Assumption : Only 2 processes are running
		if(p == pid)continue;	
		child_ctx = get_ctx_by_pid(p);
		if(child_ctx && child_ctx -> state == WAITING){
			child_ctx->state = READY;
			found = 1;
			break;
		}
	}
	if(!found)return -1;
 
	schedule(child_ctx);

//	printk("Am I here? WOW!\n");
	return 0;
}	

