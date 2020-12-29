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

void make_int(void* addr){

	//override instruction at address addr 
	u32 ins = ((u32*)addr)[0];
	ins = (ins & ~0xff) | INT3_OPCODE;
	*((u32*)addr) = ins;
}

void rem_int(void* addr){
	//override instruction at address addr 
	u32 ins = ((u32*)addr)[0];
	ins = (ins & ~0xff) | PUSHRBP_OPCODE;
	*((u32*)addr) = ins;
}

/* This is the int 0x3 handler
 * Hit from the childs context
 */
long int3_handler(struct exec_context *ctx)
{
	// Your code
	ctx->state = WAITING;
	u64 addr = ctx->regs.entry_rip - 1;
	// u64 addr = -- ctx->regs.entry_rip ;
	ctx->regs.entry_rsp -= 0x8;
	*((u64*)(ctx->regs.entry_rsp)) = ctx->regs.rbp;
	struct exec_context* p_ctx = get_ctx_by_pid(ctx->ppid);
	p_ctx->state = READY;
	p_ctx->regs.rax = (u64)addr;
	schedule(p_ctx);
	return 0;
}

/*
 * Exit handler.
 * Called on exit of Debugger and Debuggee 
 */
void debugger_on_exit(struct exec_context *ctx)
{	
	// Your code	
	if (ctx->dbg == NULL){
		//child
		struct exec_context* p_ctx = get_ctx_by_pid(ctx->ppid);
		p_ctx->regs.rax = CHILD_EXIT;
		p_ctx->state = READY;
		// schedule(p_ctx);
	}
	else{
		//parent
		// ctx->state = EXITING;
		// struct exec_context* next = pick_next_context(ctx);
		// schedule(next);
		struct breakpoint_info* it = ctx->dbg->head;
		while(it != NULL){
			struct breakpoint_info* node = it;
			it = it->next;
			free_breakpoint_info(node);
		}
		free_debug_info(ctx->dbg);
		// struct exec_context* next = pick_next_context(ctx);
		// schedule(next);
	}
}

/*
 * called from debuggers context
 * initializes debugger state
 */
int do_become_debugger(struct exec_context *ctx)
{
	// Your code
	struct debug_info* info = alloc_debug_info();
	if (!info)
		return -1;
	ctx->dbg = info;
	info->head = NULL;
	info->breakpoint_count = 0;
	info->last = 0;
	return 0;
}

/*
 * called from debuggers context
 */
int do_set_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code
	struct breakpoint_info* it = ctx->dbg->head;
	if (!it){
		struct breakpoint_info* node = alloc_breakpoint_info();
		if (!node)
			return -1;
		ctx->dbg->head = node;
		node->addr = (u64)addr;
		node->next = NULL;
		node->status = 1;
		node->num = (++ ctx->dbg->last);
		ctx->dbg->breakpoint_count++;
		make_int(addr);
		return 0;
	}

	while (it->next != NULL){
		it = it->next;
		// if address is present then enable and return
		if (it->addr == (u64)addr){
			it->status = 1;
			make_int(addr);
			return 0;
		} 
	}

	if (ctx->dbg->breakpoint_count > MAX_BREAKPOINTS - 1){
		return -1;
	}

	struct breakpoint_info* node = alloc_breakpoint_info();
	if (!node)
		return -1;
	it->next = node;
	node->addr = (u64)addr;
	node->next = NULL;
	node->status = 1;
	node->num = (++ ctx->dbg->last);
	ctx->dbg->breakpoint_count++;
	make_int(addr);
	return 0;	
}

/*
 * called from debuggers context
 */
int do_remove_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code 	
	struct breakpoint_info* it = ctx->dbg->head;
	if (!it)
		return -1;
	if (it->addr == (u64)addr){
		ctx->dbg->head = it->next;
		free_breakpoint_info(it);
		rem_int(addr);
		ctx->dbg->breakpoint_count--;
		return 0;
	}

	while(it->next != NULL && it->next->addr != (u64)addr){
		it = it->next;
	}
	if (!it->next)
		return -1;
	
	struct breakpoint_info* node = it->next;
	it->next = node->next;
	free_breakpoint_info(node);
	rem_int(addr);
	ctx->dbg->breakpoint_count--;
	return 0;
}

/*
 * called from debuggers context
 */
int do_enable_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code
	struct breakpoint_info* it = ctx->dbg->head;

	while(it != NULL && it->addr != (u64)addr)
		it = it->next;
	
	if (it == NULL)
		return -1;
	
	if (it->status)
		return 0;
	it->status = 1;
	make_int(addr);
	return 0;
}

/*
 * called from debuggers context
 */
int do_disable_breakpoint(struct exec_context *ctx, void *addr)
{
	// Your code
	struct breakpoint_info* it = ctx->dbg->head;	
	while(it != NULL && it->addr != (u64)addr)
		it = it->next;
	
	if (it == NULL)
		return -1;
	if (it->status == 0)
		return 0;
	it->status = 0;
	rem_int(addr);
	return 0;
}

/*
 * called from debuggers context
 */ 
int do_info_breakpoints(struct exec_context *ctx, struct breakpoint *ubp)
{
	// Your code
	struct breakpoint_info* it = ctx->dbg->head;
	if (ubp == NULL)
		return -1;
	int ctr = 0;
	while(it!=NULL){
		ubp[ctr].addr = it->addr;
		ubp[ctr].num = it->num;
		ubp[ctr].status = it->status;
		ctr++;
		it = it->next;
	}
	return ctr;
}

/*
 * called from debuggers context
 */
int do_info_registers(struct exec_context *ctx, struct registers *regs)
{
	// Your code
	struct exec_context* child_ctx = get_ctx_by_pid(ctx->pid + 1);
	regs->rsi = child_ctx->regs.rsi;
	regs->rdi = child_ctx->regs.rdi;
	regs->rdx = child_ctx->regs.rdx;
	regs->rcx = child_ctx->regs.rcx;
	regs->r8 = child_ctx->regs.r8;
	regs->r9 = child_ctx->regs.r9;

	regs->entry_rip = child_ctx->regs.entry_rip - 0x1;
	regs->entry_rsp = child_ctx->regs.entry_rsp + 0x8;
	regs->rbp = child_ctx->regs.rbp;
	regs->rax = child_ctx->regs.rax;
	return 0;
}

/* 
 * Called from debuggers context
 */
int do_backtrace(struct exec_context *ctx, u64 bt_buf)
{
	// Your code
	// bt_buf = (u64)os_alloc(MAX_BACKTRACE * 8);
	struct exec_context* child_ctx = get_ctx_by_pid(ctx->pid + 1);
	u64 rbp = child_ctx->regs.rbp;
	// rbp = *((u64*)rbp);
	u64 ret = *((u64*)rbp + 0x8);
	int ctr = 0;
	((u64*)bt_buf)[ctr] = ret;
	// while(ret != END_ADDR || ctr < 1){
	// 	((u64*)bt_buf)[ctr] = ret;
	// 	rbp = *((u64*)rbp);
	// 	ret = *((u64*)rbp + 0x8);
	// 	ctr++;
	// } 
	return 1;
}


/*
 * When the debugger calls wait
 * it must move to WAITING state 
 * and its child must move to READY state
 */

s64 do_wait_and_continue(struct exec_context *ctx)
{
	// Your code	
	struct exec_context* child_ctx = get_ctx_by_pid(ctx->pid + 1);
	child_ctx->state = READY;
	ctx->state = WAITING;
	schedule(child_ctx);
	return 0;
}


