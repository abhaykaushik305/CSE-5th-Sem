#include<types.h>
#include<mmap.h>

// Helper function to create a new vm_area
struct vm_area* create_vm_area(u64 start_addr, u64 end_addr, u32 flags, u32 mapping_type)
{
	struct vm_area *new_vm_area = alloc_vm_area();
	new_vm_area-> vm_start = start_addr;
	new_vm_area-> vm_end = end_addr;
	new_vm_area-> access_flags = flags;
	new_vm_area->mapping_type = mapping_type;
	new_vm_area->vm_next =  NULL;
	return new_vm_area;
}

struct vm_area* find_prev(struct exec_context *ctx,struct vm_area* iter){

	struct vm_area* prev_iter = ctx->vm_area;
	
	while(prev_iter ->vm_next!=iter)
		prev_iter = prev_iter -> vm_next;

	return prev_iter;
}

void left_merge(struct exec_context *ctx,struct vm_area* iter){
	
	struct vm_area* prev_iter = find_prev(ctx,iter);

	if(prev_iter->vm_end==iter->vm_start && prev_iter->mapping_type == iter->mapping_type && prev_iter->access_flags == iter->access_flags){
		//merge
		prev_iter->vm_end = iter->vm_end;
		prev_iter->vm_next = iter->vm_next;
		dealloc_vm_area(iter);
	}
	
	return ;
}

void right_merge(struct vm_area* iter){
	

	if(iter->vm_next->vm_start==iter->vm_end && iter->vm_next->mapping_type == iter->mapping_type && iter->vm_next->access_flags == iter->access_flags){
		
		struct vm_area* temp= iter->vm_next;
		
		iter->vm_end = temp->vm_end;
		iter->vm_next = temp->vm_next;

		dealloc_vm_area(temp);
	}
	return ;
}





/**
 * Function will invoked whenever there is page fault. (Lazy allocation)
 * 
 * For valid access. Map the physical page 
 * Return 0
 * 
 * For invalid access, i.e Access which is not matching the vm_area access rights (Writing on ReadOnly pages)
 * return -1. 
 */
int vm_area_pagefault(struct exec_context *ctx, u64 addr, int error_code)
{
	// printk("%x\n",addr);
	struct vm_area* first = ctx->vm_area;

	int found =0, map = -1;
	// ng
	while(first!=NULL){
		if(first->vm_start<=addr && first->vm_end > addr){
			found = 1;
			// check for prots
			if((error_code & 0x2) && !(first->access_flags & PROT_WRITE) )
				found = 0;
			map = first->mapping_type ;
			break;
		}
		first = first->vm_next;
	}

	if(!found)
		return -1;

	if(map == NORMAL_PAGE_MAPPING){

		u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
		u64 *entry;
		u64 pfn;
		// set User and Present flags
		// set Write flag if specified in error_code
		u64 ac_flags = 0x5 | (error_code & 0x2);
		
		// find the entry in page directory
		entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
		if(*entry & 0x1) {
			// PGD->PUD Present, access it
			pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			vaddr_base = (u64 *)osmap(pfn);
		}else{
			// allocate PUD
			pfn = os_pfn_alloc(OS_PT_REG);
			*entry = (pfn << PTE_SHIFT) | ac_flags;
			vaddr_base = osmap(pfn);
		}

		entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);

		if(*entry & 0x1) {
			// PUD->PMD Present, access it
			pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			vaddr_base = (u64 *)osmap(pfn);
		}else{
			// allocate PMD
			pfn = os_pfn_alloc(OS_PT_REG);
			*entry = (pfn << PTE_SHIFT) | ac_flags;
			vaddr_base = osmap(pfn);
		}

		entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
		if(*entry & 0x1) {
			// PMD->PTE Present, access it
			pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			vaddr_base = (u64 *)osmap(pfn);
		}else{
			// allocate PLD 
			pfn = os_pfn_alloc(OS_PT_REG);
			*entry = (pfn << PTE_SHIFT) | ac_flags;
			vaddr_base = osmap(pfn);
		}

		entry = vaddr_base + ((addr & PTE_MASK) >> PTE_SHIFT);
		if(*entry & 0x1){
			*entry = *entry | ac_flags;
		}
		else{
			pfn = os_pfn_alloc(USER_REG);
			*entry = (pfn << PTE_SHIFT) | ac_flags;	
		}
		// since this fault occured as frame was not present, we don't need present check here
		// pfn = os_pfn_alloc(USER_REG);
		// *entry = (pfn << PTE_SHIFT) | ac_flags;

	}

	if(map == HUGE_PAGE_MAPPING){

		u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
		u64 *entry;
		u64 pfn;
		// set User and Present flags
		// set Write flag if specified in error_code
		u64 ac_flags = 0x5 | (error_code & 0x2);
		
		// find the entry in page directory
		entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
		if(*entry & 0x1) {
			// PGD->PUD Present, access it
			pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			vaddr_base = (u64 *)osmap(pfn);
		}else{
			// allocate PUD
			pfn = os_pfn_alloc(OS_PT_REG);
			*entry = (pfn << PTE_SHIFT) | ac_flags;
			vaddr_base = osmap(pfn);
		}

		entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
		if(*entry & 0x1) {
			// PUD->PMD Present, access it
			pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			vaddr_base = (u64 *)osmap(pfn);
		}else{
			// allocate PMD
			pfn = os_pfn_alloc(OS_PT_REG);
			*entry = (pfn << PTE_SHIFT) | ac_flags;
			vaddr_base = osmap(pfn);
		}

		entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
		
		pfn = get_hugepage_pfn(os_hugepage_alloc());

		*entry = (pfn << HUGEPAGE_SHIFT) | ac_flags | 0x80;
		
	}



	return 1;
}







/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
	
	if(length % 4096){
		length = (length/4096 + 1) * 4096 ; 
	}
	if(current->vm_area==NULL){
		struct vm_area* dummy = create_vm_area(MMAP_AREA_START,MMAP_AREA_START + 0x1000 ,0x4,NORMAL_PAGE_MAPPING);
		current->vm_area = dummy;
	}
	if(flags == MAP_FIXED && (!addr  || addr % 0x1000)){
		return -1;
	}

	if(flags == MAP_FIXED){
		struct vm_area* iter = current->vm_area;


		while(iter->vm_next!=NULL &&  iter->vm_next->vm_start < addr){
			iter=iter->vm_next;
		}

		// itr is last node
		if(iter->vm_next==NULL){
			
			if(iter->vm_end < addr){
				struct vm_area* node = create_vm_area(addr,addr+length,prot,NORMAL_PAGE_MAPPING);
				iter->vm_next = node;
				return addr;
			}
			if(iter->vm_end==addr){
				if(iter->access_flags == prot && iter->mapping_type == NORMAL_PAGE_MAPPING){
					iter->vm_end = addr + length;
					return addr;
				}

				struct vm_area* node = create_vm_area(addr,addr+length,prot,NORMAL_PAGE_MAPPING);
				iter->vm_next = node;
				
				return addr;	

			}

			// addr > start and addr < end
			return -1;	
		}

		// between 1 and 2
		else{
			if(addr < iter->vm_end)
				return -1;
			if(addr == iter->vm_end){
				// overlap into next
				if(addr+length > iter->vm_next->vm_start )
					return -1;
				// exactly fit betweeen two
				if(addr + length == iter->vm_next->vm_start){
					// triple merge]
					if(iter->access_flags == prot && prot == iter->vm_next->access_flags){
						
						struct vm_area* temp = iter->vm_next;
						iter->vm_end = temp->vm_end;
						iter->vm_next = temp->vm_next;
						dealloc_vm_area(temp);

						return addr;
					}
					// merge into 1
					if(iter->access_flags == prot && iter->mapping_type == NORMAL_PAGE_MAPPING){
						iter->vm_end = addr+length;
						return addr;
					}
					//merge into 2
					if(iter->vm_next->access_flags == prot && iter->vm_next->mapping_type == NORMAL_PAGE_MAPPING){
						iter->vm_next->vm_start = addr;
						return addr;
					}
					// no merge
					struct vm_area* node = create_vm_area(addr,addr+length,prot,NORMAL_PAGE_MAPPING);
					node->vm_next = iter->vm_next;
					iter->vm_next = node;
					return addr;
				} 

				if(addr + length < iter->vm_next->vm_start){

					// merge into 1
					if(iter->access_flags == prot && iter->mapping_type == NORMAL_PAGE_MAPPING ){
						iter->vm_end = addr+length;
						return addr;
					}
					// no merge
					struct vm_area* node = create_vm_area(addr,addr+length,prot,NORMAL_PAGE_MAPPING);
					node->vm_next = iter->vm_next;
					iter->vm_next = node;
					return addr;	

				}
			}
			if(addr > iter->vm_end){
				
				if(addr+length > iter->vm_next->vm_start )
					return -1;
				
				if(addr + length == iter->vm_next->vm_start){
					
					//merge into 2
					if(iter->vm_next->access_flags == prot && iter->vm_next->mapping_type == NORMAL_PAGE_MAPPING){
						iter->vm_next->vm_start = addr;
						return addr;
					}
					// no merge
					struct vm_area* node = create_vm_area(addr,addr+length,prot,NORMAL_PAGE_MAPPING);
					node->vm_next = iter->vm_next;
					iter->vm_next = node;
					return addr;
				} 

				if(addr + length < iter->vm_next->vm_start){

					// no merge
					struct vm_area* node = create_vm_area(addr,addr+length,prot,NORMAL_PAGE_MAPPING);
					node->vm_next = iter->vm_next;
					iter->vm_next = node;
					return addr;	

				}
			}
		}

	}

	else{
		//flags is 0.
		
		// ALIGN THE ADDR
		if(addr && addr % 4096){
			addr = (addr/4096 + 1) * 4096 ; 
		}

		struct vm_area* iter = current->vm_area;

		int myflag=0; // 

		if(addr ){

			// make vm_area at hint address if possible

			while(iter->vm_next!=NULL &&  iter->vm_next->vm_start < addr){
				iter=iter->vm_next;
			}

			if(iter->vm_next == NULL){
				if(addr > iter->vm_end){
					struct vm_area* node = create_vm_area(addr,addr+length,prot,NORMAL_PAGE_MAPPING);
					iter->vm_next = node;
					return addr;
				}
				if(iter->vm_end==addr){
					if(iter->access_flags == prot && iter->mapping_type == NORMAL_PAGE_MAPPING){
						iter->vm_end = addr + length;
						return addr;
					}

				struct vm_area* node = create_vm_area(addr,addr+length,prot,NORMAL_PAGE_MAPPING);
				iter->vm_next = node;
				
				return addr;	

				}
				// addr is already mapped.
				myflag=1;
				
			}

			else{

				if(addr == iter->vm_end){
					
					
					// exactly fit betweeen two
					if(addr + length == iter->vm_next->vm_start){
						// triple merge
						if(iter->access_flags == prot && prot == iter->vm_next->access_flags && iter->mapping_type == NORMAL_PAGE_MAPPING && iter->vm_next->mapping_type== NORMAL_PAGE_MAPPING){
							
							struct vm_area* temp = iter->vm_next;
							iter->vm_end = temp->vm_end;
							iter->vm_next = temp->vm_next;
							dealloc_vm_area(temp);

							return addr;
						}
						// merge into 1
						if(iter->access_flags == prot && iter->mapping_type== NORMAL_PAGE_MAPPING){
							iter->vm_end = addr+length;
							return addr;
						}
						//merge into 2
						if(iter->vm_next->access_flags == prot && iter->vm_next->mapping_type == NORMAL_PAGE_MAPPING){
							iter->vm_next->vm_start = addr;
							return addr;
						}
						// no merge
						struct vm_area* node = create_vm_area(addr,addr+length,prot,NORMAL_PAGE_MAPPING);
						node->vm_next = iter->vm_next;
						iter->vm_next = node;
						return addr;
					} 

					if(addr + length < iter->vm_next->vm_start){

						// merge into 1
						if(iter->access_flags == prot && iter->mapping_type== NORMAL_PAGE_MAPPING){
							iter->vm_end = addr+length;
							return addr;
						}
						// no merge
						struct vm_area* node = create_vm_area(addr,addr+length,prot,NORMAL_PAGE_MAPPING);
						node->vm_next = iter->vm_next;
						iter->vm_next = node;
						return addr;	

					}

					// overlap into next
					if(addr+length > iter->vm_next->vm_start )
						myflag=1;

				}
				if(addr > iter->vm_end){
					
					if(addr+length > iter->vm_next->vm_start )
						myflag=1;
					
					if(addr + length == iter->vm_next->vm_start){
					
					//merge into 2
						if(iter->vm_next->access_flags == prot && iter->vm_next->mapping_type == NORMAL_PAGE_MAPPING){
							iter->vm_next->vm_start = addr;
							return addr;
						}
						// no merge
						struct vm_area* node = create_vm_area(addr,addr+length,prot,NORMAL_PAGE_MAPPING);
						node->vm_next = iter->vm_next;
						iter->vm_next = node;
						return addr;
					} 

					if(addr + length < iter->vm_next->vm_start){

						// no merge
						struct vm_area* node = create_vm_area(addr,addr+length,prot,NORMAL_PAGE_MAPPING);
						node->vm_next = iter->vm_next;
						iter->vm_next = node;
						return addr;	

					}
				}

				if(addr < iter->vm_end){
					myflag=1;
				}
			}
		}

		// add the vm_area at first available free slot
		iter = current->vm_area;
		while( iter->vm_next!=NULL  && iter->vm_next->vm_start - iter->vm_end < length){
			iter = iter->vm_next;
		}

		if(iter->vm_next == NULL){
			if(iter->access_flags == prot && iter->mapping_type == NORMAL_PAGE_MAPPING){
				iter->vm_end = iter->vm_end + length;
				return iter->vm_end - length;
			}
			// printk("Hi i am here");
			struct vm_area* node = create_vm_area(iter->vm_end,iter->vm_end+length,prot,NORMAL_PAGE_MAPPING);
			iter->vm_next = node;
			return iter->vm_end;
		}

		else{
			if(iter->vm_next->vm_start - iter->vm_end == length){
				if(iter->access_flags == prot && prot == iter->vm_next->access_flags && iter->mapping_type == NORMAL_PAGE_MAPPING && iter->vm_next->mapping_type== NORMAL_PAGE_MAPPING){
					
					struct vm_area* temp = iter->vm_next;
					long res = iter->vm_end;
					iter->vm_end = temp->vm_end;
					iter->vm_next = temp->vm_next;
					dealloc_vm_area(temp);

					return res;
				}
				// merge into 1
				if(iter->access_flags == prot && iter->mapping_type == NORMAL_PAGE_MAPPING){
					iter->vm_end = iter->vm_end + length;
					return iter->vm_end - length;
				}
				//merge into 2
				if(iter->vm_next->access_flags == prot && iter->vm_next->mapping_type == NORMAL_PAGE_MAPPING){
					iter->vm_next->vm_start = iter->vm_next->vm_start - length;
					return iter->vm_next->vm_start;
				}
				// no merge
				struct vm_area* node = create_vm_area(iter->vm_end,iter->vm_end+length,prot,NORMAL_PAGE_MAPPING);
				node->vm_next = iter->vm_next;
				iter->vm_next = node;
				return iter->vm_end;
			}

			// space is strictly greater than length

			if(iter->vm_next->vm_start - iter->vm_end > length){

				// merge into 1
				if(iter->access_flags == prot && iter->mapping_type == NORMAL_PAGE_MAPPING){
					iter->vm_end = iter->vm_end + length;
					return iter->vm_end - length;
				}
				// no merge
				struct vm_area* node = create_vm_area(iter->vm_end,iter->vm_end+length,prot,NORMAL_PAGE_MAPPING);
				node->vm_next = iter->vm_next;
				iter->vm_next = node;
				return iter->vm_end;

			}
		}


	}

	return 0;
}


/**
 * munmap system call implemenations
 */

int delete_if_physical_allocated(struct exec_context *ctx, u64 addr){

		u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
		u64 *entry;
		u64 pfn;
		
		// find the entry in page directory
		entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
		if(*entry & 0x1) {
			// PGD->PUD Present, access it
			pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			vaddr_base = (u64 *)osmap(pfn);
		}
		else
			return 0;
		

		entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);

		if(*entry & 0x1) {
			// PUD->PMD Present, access it
			pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			vaddr_base = (u64 *)osmap(pfn);
		}
		else
			return 0;			
		
		entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
		if(*entry & 0x1) {
			// PMD->PTE Present, access it
			pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			vaddr_base = (u64 *)osmap(pfn);
		}
		else
			return 0;

		entry = vaddr_base + ((addr & PTE_MASK) >> PTE_SHIFT);
		
		if(*entry & 0x1) {
			pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			*entry = *entry & ~(0x1);
			os_pfn_free(USER_REG,pfn);

		}
		else{
			return 0;
		}

		asm volatile (
		  "invlpg (%0);" 
		  :: "r"(addr) 
		  : "memory"
		 ); 

		return 1;
}

void delete_physical(struct exec_context *current,u64 start,u64 end){

	int ans=0;
	for(u64 i = start; i<end ; i+= 0x1000){
		ans = delete_if_physical_allocated(current,i);
	}
	return ;
}

int delete_if_physical_allocated_huge(struct exec_context *ctx,u64 addr){
	u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
	u64 *entry;
	u64 pfn;
	
	// find the entry in page directory
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	if(*entry & 0x1){

		*entry = *entry & ~(0x1) & ~(0x80);
		// pfn = (*entry >> HUGEPAGE_SHIFT) & 0xFFFFFFFF;
		// os_hugepage_free();
		vaddr_base = (u64*)( (*entry >> 21) << 21 );
		
		os_hugepage_free(vaddr_base);

		// set last and 7th bit 0

		
	}
	else{
		return 0;
	}
	//
	asm volatile (
	  "invlpg (%0);" 
	  :: "r"(addr) 
	  : "memory"
	 ); 
	return 1;

}

void delete_physical_huge(struct exec_context *current,u64 start,u64 end){
	int ans = 0;
	for(u64 i = start;i<end ; i+= 0x200000){
		ans = delete_if_physical_allocated_huge(current,i);
	}
	return ;

}

int vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
	// addr should be aligned
	if(addr % 4096){
		return -1;
	}

	
	
	struct vm_area* iter= current->vm_area;

	
	while(iter->vm_next!=NULL && iter->vm_next->vm_start < addr){
		iter=iter->vm_next;
		// printk("iter = %x\n",iter->vm_start);
	}
	// printk("iter = %x\n",iter->vm_start);
	if(iter->vm_next==NULL){
		// addr beyond vm_end
		if(addr >= iter->vm_end){
			//simply return
			return 0;
		}
		if(addr < iter->vm_end){
			

			if(iter -> mapping_type == NORMAL_PAGE_MAPPING){

				if(length % 4096){
					length = (length/4096 + 1)*4096; 
				}

				if(addr + length < iter->vm_end){
					// split 
					delete_physical(current,addr,addr+length);
					struct vm_area* node = create_vm_area(addr+length,iter->vm_end,iter->access_flags,NORMAL_PAGE_MAPPING);
					iter->vm_end = addr;
					iter->vm_next = node;
					return 0;
				}
				else{
					//shrink
					delete_physical(current,addr,iter->vm_end);
					iter->vm_end = addr; 
					return 0;
				}

			}
			else{
				// HUGE PAGE MAPPING

				// corresponding start and ends
				u64 pred_start = iter->vm_start + ((addr - iter->vm_start)/0x200000)*0x200000 ;

				u64 pred_end = addr + length;
				if( (addr+length - iter->vm_start) % 0x200000)
					pred_end = iter->vm_start + ((addr+length - iter->vm_start)/0x200000 + 1)*0x200000;

				
				// find prev node
				struct vm_area* iter2 =  find_prev(current,iter);


				if(pred_end >= iter->vm_end){
					
					if(pred_start == iter->vm_start){
						// dealloc the last node
						delete_physical_huge(current,iter->vm_start,iter->vm_end);
						iter2->vm_next = NULL;
						dealloc_vm_area(iter);
						return 0;
					}
					else{
						//shrink left side
						delete_physical_huge(current,pred_start,iter->vm_end);
						iter->vm_end = pred_start ;
						return 0;
					}
				}
				else{

					if(pred_start == iter->vm_start){
						// shrink right side
						delete_physical_huge(current,iter->vm_start,pred_end);
						iter->vm_start = pred_end;
						return 0;
					}
					else{
						// split
						delete_physical_huge(current,pred_start,pred_end);
						iter-> vm_end = pred_start;
						struct vm_area* node = create_vm_area(pred_end,iter->vm_end,iter->access_flags,HUGE_PAGE_MAPPING);
						iter->vm_next = node;
						return 0;
					}

				}
			}
		}


	}
	
	else{
		// now addr is strictly after iter->vm_start but before or equal with iter->vm_next->vm_start
		struct vm_area* iter2 = iter;
		while(iter2!=NULL && iter2->vm_end < addr + length ){
			iter2 = iter2->vm_next;
		}


		
		if(addr >= iter->vm_end){

			if(iter2==NULL){
				// deallloc all after iter
				
				struct vm_area* temp1 = iter->vm_next, *temp2 ;
				while(temp1!=NULL){
					
					temp2=temp1->vm_next;
					if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
						delete_physical(current,temp1->vm_start,temp1->vm_end);
					else{
						delete_physical_huge(current,temp1->vm_start,temp1->vm_end);
						// to be entered soon
					}
					dealloc_vm_area(temp1);
					temp1=temp2;
					
				}
				iter->vm_next = NULL;
				return 0;
			}

			else{

				// ending is not mapped
				if(iter2->vm_start >= addr + length){
					// simply dealloc from iter->vm_next to iter 2->prev
					struct vm_area* temp1 = iter->vm_next,*temp2 = iter->vm_next;
					while(temp1!=iter2){
						temp2=temp1->vm_next;
						if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
							delete_physical(current,temp1->vm_start,temp1->vm_end);
						else{
							delete_physical_huge(current,temp1->vm_start,temp1->vm_end);
							// to be entered soon
						}
						dealloc_vm_area(temp1);
						temp1=temp2;
					}
					iter->vm_next = iter2;
					return 0;

				}

				// end region mapped
				u64 pred_end;

				if(iter2->mapping_type == NORMAL_PAGE_MAPPING){
					if(length % 4096){
						length = (length/4096 + 1)*4096;
					}
					pred_end = addr+length;
					
					if(pred_end == iter2->vm_end){
						// delete iter->vm_next till iter2

						// printk("I am here : munmap");
						struct vm_area* temp1 = iter->vm_next,*temp2 ;
						// printk("%x\n",temp1->vm_start);
						iter->vm_next=iter2->vm_next;
						// dealloc_vm_area(iter2);
						while(temp1!=iter2->vm_next){
							if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
								delete_physical(current,temp1->vm_start,temp1->vm_end);
							else{
								delete_physical_huge(current,temp1->vm_start,temp1->vm_end);
								// to be entered soon
							}
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}
						
						return 0;
					}
					else{
						// iter2 vm area going to split

						

						struct vm_area* temp1 = iter->vm_next,*temp2;
						while(temp1!=iter2){
							if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
								delete_physical(current,temp1->vm_start,temp1->vm_end);
							else{
								delete_physical_huge(current,temp1->vm_start,temp1->vm_end);
								// to be entered soon
							}
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}
						// shrink iter2
						if(iter2->mapping_type == NORMAL_PAGE_MAPPING)
							delete_physical(current,iter2->vm_start,pred_end);
						else
							delete_physical_huge(current,iter2->vm_start,pred_end);
						iter2->vm_start = pred_end;
						iter->vm_next = iter2; 
						return 0;

					}
					return 0;
				}
				else{
					//  HUGE MAPPING 

					if( (addr+length - iter2->vm_start) % 0x200000)
						pred_end = iter->vm_start + ((addr+length - iter->vm_start)/0x200000 + 1)*0x200000;

					else 
						pred_end = iter->vm_start + ((addr+length - iter->vm_start)/0x200000)*0x200000 ;

					if(pred_end == iter2->vm_end){
						// delete iter->vm_next till iter2

						
						struct vm_area* temp1 = iter->vm_next,*temp2;
						


						iter->vm_next = iter2->vm_next;
						
						while(temp1!=iter2->vm_next){
							if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
								delete_physical(current,temp1->vm_start,temp1->vm_end);
							else{
								delete_physical_huge(current,temp1->vm_start,temp1->vm_end);
								// to be entered soon
							}
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}

						// dealloc_vm_area(iter2);
					}
					else{
						// iter2 vm area going to split
						
						struct vm_area* temp1 = iter->vm_next,*temp2 ;
						while(temp1!=iter2){
							if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
								delete_physical(current,temp1->vm_start,temp1->vm_end);
							else{
								delete_physical_huge(current,temp1->vm_start,temp1->vm_end);
								// to be entered soon
							}
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}
						// shrink iter2
						delete_physical_huge(current,iter->vm_start,pred_end);
						iter->vm_next = iter2; 
						iter2->vm_start = pred_end;

					}
					return 0;
				}
				
			}
		}


		else{

			// 1 is going to split or vanish 
			u64 pred_start = addr,pred_end;

			if(iter->mapping_type == HUGE_PAGE_MAPPING){
				pred_start = iter->vm_start + ((addr - iter->vm_start)/0x200000)*0x200000 ;
			}

			if(iter2==NULL){
				struct vm_area* temp1 = iter->vm_next, *temp2 ;
				while(temp1){
					if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
						delete_physical(current,temp1->vm_start,temp1->vm_end);
					else{
						delete_physical_huge(current,temp1->vm_start,temp1->vm_end);	
							// to be entered soon
					}
					temp2=temp1->vm_next;
					dealloc_vm_area(temp1);
					temp1=temp2;
				}
				
				if(iter->mapping_type == NORMAL_PAGE_MAPPING)
						delete_physical(current,pred_start,iter->vm_end);
				else{
						delete_physical_huge(current,pred_start,iter->vm_end);
							// to be entered soon
				}
				iter->vm_end=pred_start;
				iter->vm_next = NULL;
				return 0;
			}

			else{

				if(iter2->vm_start >= addr + length){
					// iter 2 will remain as it is

					// iter will vanish
					if(pred_start == iter->vm_start){
						// we need to find previous of iter
						struct vm_area* prev_iter= current->vm_area;
						while(prev_iter->vm_next != iter){
							prev_iter = prev_iter -> vm_next;
						}

						prev_iter->vm_next = iter2; 

						struct vm_area* temp1 = iter,*temp2;
						while(temp1!=iter2){
							if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
								delete_physical(current,temp1->vm_start,temp1->vm_end);
							else{
								delete_physical_huge(current,temp1->vm_start,temp1->vm_end);		
									// to be entered soon
							}
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}

						return 0;
						
					}

					// iter will split
					struct vm_area* temp1 = iter->vm_next,*temp2 = iter->vm_next;
					while(temp1!=iter2){
						if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
							delete_physical(current,temp1->vm_start,temp1->vm_end);
						else{
							delete_physical_huge(current,temp1->vm_start,temp1->vm_end);
								// to be entered soon
						}
						temp2=temp1->vm_next;
						dealloc_vm_area(temp1);
						temp1=temp2;
					}
					if(iter->mapping_type == NORMAL_PAGE_MAPPING)
						delete_physical(current,pred_start,iter->vm_end);
					else{
						delete_physical_huge(current,pred_start,iter->vm_end);
							// to be entered soon
					}
					iter->vm_end = pred_start;
					iter->vm_next = iter2;
					return 0;

				}

				// end region mapped

				if(iter2->mapping_type == NORMAL_PAGE_MAPPING){
					if(length % 4096){
						length = (length/4096 + 1)*4096;
					}
					pred_end = addr+length;	
				}
				else{
					if( (addr+length - iter2->vm_start) % 0x200000)
						pred_end = iter2->vm_start + ((addr+length - iter2->vm_start)/0x200000 + 1)*0x200000;

					else 
						pred_end = iter2->vm_start + ((addr+length - iter2->vm_start)/0x200000)*0x200000 ;
				}


				if(pred_end == iter2->vm_end){
					
					// iter and iter 2 both needs to be deleted
					if(pred_start == iter->vm_start){
						// we need to find previous of iter
						struct vm_area* prev_iter= find_prev(current,iter);

						
						// iter and iter2 can be same
						if(iter == iter2){

							prev_iter->vm_next = iter2->vm_next;
							if(iter->mapping_type == NORMAL_PAGE_MAPPING)
								delete_physical(current,iter->vm_start,iter->vm_end);
							else{
								delete_physical_huge(current,iter->vm_start,iter->vm_end);
									// to be entered soon
							}
							dealloc_vm_area(iter);
							return 0;
						}	
						struct vm_area* temp1 = iter,*temp2;
						while(temp1!=iter2){
							if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
								delete_physical(current,temp1->vm_start,temp1->vm_end);
							else{
								delete_physical_huge(current,temp1->vm_start,temp1->vm_end);
									// to be entered soon
							}
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}


						prev_iter->vm_next = iter2->vm_next;
						
						if(iter2->mapping_type == NORMAL_PAGE_MAPPING)
								delete_physical(current,iter2->vm_start,iter2->vm_end);
						else{
								delete_physical_huge(current,iter2->vm_start,iter2->vm_end);
									// to be entered soon
						}
						dealloc_vm_area(iter2);
						return 0;
					}

					
					struct vm_area* temp1 = iter->vm_next,*temp2;
					while(temp1!=iter2){
						if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
							delete_physical(current,temp1->vm_start,temp1->vm_end);
						else{
							delete_physical_huge(current,temp1->vm_start,temp1->vm_end);
								// to be entered soon
						}
						temp2=temp1->vm_next;
						dealloc_vm_area(temp1);
						temp1=temp2;
					}
					
					// 1 will shrink and 2 gets deleted
					if(iter->mapping_type == NORMAL_PAGE_MAPPING)
						delete_physical(current,pred_start,iter->vm_end);
					else{
						delete_physical_huge(current,pred_start,iter->vm_end);	// to be entered soon
					}

					iter->vm_end = pred_start;

					//iter and iter2 can be same 
					if(iter!=iter2){
						iter->vm_next = iter2->vm_next;
						
						if(iter2->mapping_type == NORMAL_PAGE_MAPPING)
							delete_physical(current,iter2->vm_start,iter2->vm_end);
						else{
							delete_physical_huge(current,iter2->vm_start,iter2->vm_end);
								// to be entered soon
						}

						dealloc_vm_area(iter2);
					}
					return 0;
				}
				else{
					

					// iter2 for sure going to shrink

					// 1 deleted 
					if(pred_start == iter->vm_start){
						
						// iter and iter2 can be same
						if(iter == iter2){
							// simply shrink it
							if(iter->mapping_type == NORMAL_PAGE_MAPPING)
								delete_physical(current,pred_start,pred_end);
							else{
								delete_physical_huge(current,pred_start,pred_end);
									// to be entered soon
							}
							iter2->vm_start = pred_end;
							return 0;
						}

						// we need to find previous of iter
						struct vm_area* prev_iter= find_prev(current,iter);

						
						
						struct vm_area* temp1 = iter,*temp2;
						while(temp1!=iter2){
							if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
								delete_physical(current,temp1->vm_start,temp1->vm_end);
							else{
								delete_physical_huge(current,temp1->vm_start,temp1->vm_end);
									// to be entered soon
							}
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}

						if(iter2->mapping_type == NORMAL_PAGE_MAPPING)
							delete_physical(current,iter2->vm_start,pred_end);
						else{
							delete_physical_huge(current,iter2->vm_start,pred_end);
								// to be entered soon
						}
						iter2->vm_start = pred_end;
						prev_iter->vm_next = iter2; 
						return 0;
					}

					
					if(iter == iter2){

						// there will be split bro 

						struct vm_area *node = create_vm_area(pred_end,iter->vm_end,iter->access_flags,iter->mapping_type);
						if(iter->mapping_type == NORMAL_PAGE_MAPPING)
							delete_physical(current,pred_start,pred_end);
						else{
							delete_physical_huge(current,pred_start,pred_end);
								// to be entered soon
						}
						iter->vm_end = pred_start;
						node->vm_next = iter->vm_next;
						iter->vm_next  = node;
						return 0;
					}
					
					
					struct vm_area* temp1 = iter->vm_next,*temp2 = iter->vm_next;
					while(temp1!=iter2){
						if(temp1->mapping_type == NORMAL_PAGE_MAPPING)
							delete_physical(current,temp1->vm_start,temp1->vm_end);
						else{
							delete_physical_huge(current,temp1->vm_start,temp1->vm_end);
								// to be entered soon
						}
						temp2=temp1->vm_next;
						dealloc_vm_area(temp1);
						temp1=temp2;
					}

					if(iter->mapping_type == NORMAL_PAGE_MAPPING)
							delete_physical(current,pred_start,iter->vm_end);
					else{
							delete_physical_huge(current,pred_start,iter->vm_end);
							// to be entered soon
					
					}
					if(iter2->mapping_type == NORMAL_PAGE_MAPPING)
						delete_physical(current,iter2->vm_start,pred_end);
					else{
						delete_physical_huge(current,iter2->vm_start,pred_end);
							// to be entered soon
					}	
					// iter also shrink 
					iter->vm_end = pred_start;
					// shrink iter2
					iter2->vm_start = pred_end;

					iter->vm_next = iter2;

				}
				return 0;
			

			}
		}


	}


	return 0;
}




/**
 * make_hugepage system call implemenation
 */

void install_hugepage_pagetable(struct exec_context *ctx, char* buff,u32 prot,u64 addr){

	u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
	u64 *entry;
	u64 pfn;
	// set User and Present flags
	// set Write flag if specified in error_code
	u64 ac_flags = 0x5 | (prot & 0x2);
	
	// find the entry in page directory
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PUD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PMD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	
	pfn = get_hugepage_pfn(buff);

	*entry = (pfn << HUGEPAGE_SHIFT) | ac_flags | 0x80;
	
	return ;	
} 

int check_if_physical_allocated(struct exec_context *ctx, u64 addr){

		u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
		u64 *entry;
		u64 pfn;
		
		// find the entry in page directory
		entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
		if(*entry & 0x1) {
			// PGD->PUD Present, access it
			pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			vaddr_base = (u64 *)osmap(pfn);
		}
		else
			return 0;
		

		entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);

		if(*entry & 0x1) {
			// PUD->PMD Present, access it
			pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			vaddr_base = (u64 *)osmap(pfn);
		}
		else
			return 0;			
		
		entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
		if(*entry & 0x1) {
			// PMD->PTE Present, access it
			pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			vaddr_base = (u64 *)osmap(pfn);
		}
		else
			return 0;

		entry = vaddr_base + ((addr & PTE_MASK) >> PTE_SHIFT);
		
		if(*entry & 0x1) {
			// PMD->PTE Present, access it
			// pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
			
		}
		else{
			return 0;
		}

		return 1;
}

void handle_physical_makehugemap(struct exec_context *current,u64 start,u64 end, u32 prot){

	// int ans=0;
	// printk("%Handle MakeHuge map x %x\n",start,end);
	for(u64 i = start;i < end ; i+= 0x200000){
		// int flag=0;
		// for(u64 j = i;j<i+0x200000;j+=0x1000){
		// 	if(check_if_physical_allocated(current,j)){
		// 		flag=1;
		// 		break;
		// 	}
		// }
		// if(flag){
		// 	void* buff = os_hugepage_alloc();
		// 	for(u64 j = 0; j < 512  ;j++){
		// 		// printk("I am khare!\n");

		// 		memcpy((char*)(buff+j*4096),(char*)(i + j*4096),0x1000);
		// 		// printk("I am khare2!\n");
		// 		delete_if_physical_allocated(current,i+j*0x1000);
				
		// 	}
			

		// 	install_hugepage_pagetable(current,buff,prot,i);
		// }
			
		void * buff = os_hugepage_alloc();
		int flag = 0;
		for(u64 j = i; j<i+0x200000;j+=0x1000){
			if(check_if_physical_allocated(current,j)){
				memcpy((char*)(buff+j-i),(char*)(j),4096);
				delete_if_physical_allocated(current,j);
				flag=1;
			} 	 
		}
		// printk("Outside loop %x\n",i);
		if(flag){
			install_hugepage_pagetable(current,buff,prot,i);
		}
		else{
			os_hugepage_free(buff);
		}
	}
	return ;
}

long vm_area_make_hugepage(struct exec_context *current, void *inaddr, u32 length, u32 prot, u32 force_prot)
{
	u64 new_start = (u64) inaddr, new_end = (u64) inaddr + length;

	u64 addr = (u64) inaddr;
	if(inaddr == NULL)
		return -EINVAL;

	if(new_start % 0x200000){
		new_start = (new_start /0x200000 + 1)*0x200000; 
	}
	if(new_end % 0x200000){
		new_end = (new_end/0x200000 )*0x200000 ;
	}

	if(new_start >= new_end){
		return -EINVAL;
	}

	// printk("new_start = %x new_end = %x\n",new_start,new_end);
		// check for conditions first --- I asked on Piazza

	int no_map=0,huge_map=0,for_prot=0;


	struct vm_area *iter = current->vm_area;

	while(iter!=NULL && iter->vm_end <= addr)
		iter=iter->vm_next;

	if(iter==NULL || iter->vm_start > addr)
		return -ENOMAPPING;

	while(iter!=NULL && iter->vm_start  < addr + length){
		
		if(iter->vm_next == NULL && iter->vm_end < addr + length)
			no_map=1;

		// check for gap
		if(iter->vm_next!=NULL && iter->vm_next->vm_start!=iter->vm_end)
			no_map=1;
		if(iter->mapping_type == HUGE_PAGE_MAPPING)
			huge_map = 1;
		if(force_prot==0 && iter->access_flags!=prot)
			for_prot=1;
		iter = iter->vm_next;
	}

	if(no_map)
		return -ENOMAPPING;
	if(huge_map)
		return  -EVMAOCCUPIED;
	if(force_prot == 0 && for_prot==1)
		return -EDIFFPROT;



	iter= current->vm_area;
	struct vm_area  *first = NULL,*second = NULL;

	// This time small the code bro!!!


	while(iter!=NULL && second == NULL){

		if(first==NULL && new_start >= iter->vm_start && new_start < iter->vm_end){
			first = iter;
		}

		if(second==NULL && new_end > iter->vm_start && new_end <= iter->vm_end){
			second = iter;
		}		

		iter=iter->vm_next;
	}

	if(first->vm_start == new_start && second->vm_end == new_end){
		// simply delete all nodes from first->next to second


		// printk("I am here Abhay");
		struct vm_area *temp1 = first->vm_next,*temp2;
		

		first->vm_next = second->vm_next;
		
		first->mapping_type = HUGE_PAGE_MAPPING;
		first->access_flags = prot;
		first->vm_end = new_end;

		

		while(temp1!=second->vm_next){
			temp2=temp1->vm_next;
			dealloc_vm_area(temp1);
			temp1=temp2;
		}

		handle_physical_makehugemap(current,first->vm_start,first->vm_end,prot);

		// expand the first
		
		
		right_merge(first);
		left_merge(current,first);

		return new_start;
	}

	if(first->vm_start == new_start && second->vm_end != new_end){

		if(first == second){
			// no deletion
			// new node created
			struct vm_area *node= create_vm_area(new_end,second->vm_end,second->access_flags,NORMAL_PAGE_MAPPING);
			
			// shrink the first

			first->mapping_type = HUGE_PAGE_MAPPING;
			first->access_flags = prot;
			first->vm_end = new_end;
			node->vm_next = first->vm_next;
	 		first->vm_next = node;	

	 		handle_physical_makehugemap(current,first->vm_start,first->vm_end,prot);

	 		left_merge(current,first);	 
	 		return new_start;
		}


		struct vm_area *temp1 = first->vm_next,*temp2;
		
		first->vm_next = second;
		
		first->mapping_type = HUGE_PAGE_MAPPING;
		first->access_flags = prot;
		first->vm_end = new_end;

		handle_physical_makehugemap(current,first->vm_start,first->vm_end,prot);

		while(temp1!=second){
			temp2=temp1->vm_next;
			dealloc_vm_area(temp1);
			temp1=temp2;
		}

		// expand the first
		

		// shrink the second
		second->vm_start = new_end;
		left_merge(current,first);
		return new_start;

	}

	if(first->vm_start != new_start && second->vm_end == new_end){

		if(first == second){
			// no deletion
			// new node created
			struct vm_area *node= create_vm_area(new_start,second->vm_end,prot,HUGE_PAGE_MAPPING);
			
			// shrink the first
			first->vm_end = new_start;
			node->vm_next = first->vm_next;
	 		first->vm_next = node;

	 		handle_physical_makehugemap(current,node->vm_start,node->vm_end,prot);		 
	 		right_merge(node);
	 		return new_start;
		}


		struct vm_area *temp1 = first->vm_next,*temp2;
		
		first->vm_next = second;
		

		// shrink the first
		first->vm_end = new_start;

		// expand the second
		second->vm_start = new_start;
		second->mapping_type = HUGE_PAGE_MAPPING;
		second->access_flags = prot;

		handle_physical_makehugemap(current,second->vm_start,second->vm_end,prot);

		while(temp1!=second){
			temp2=temp1->vm_next;
			dealloc_vm_area(temp1);
			temp1=temp2;
		}

		
		
		right_merge(second);
		return new_start;

	}

	
	if(first->vm_start != new_start && second->vm_end != new_end){

		
		if(first == second){
			// no deletion
			// 2 new node created
			struct vm_area *node1= create_vm_area(new_start,new_end,prot,HUGE_PAGE_MAPPING);
			struct vm_area *node2= create_vm_area(new_end,first->vm_end,first->access_flags,NORMAL_PAGE_MAPPING);
			
			first->vm_end = new_start;
				
			node1->vm_next = node2;	
			node2->vm_next = first->vm_next;

	 		first->vm_next = node1;
	 		// printk("I am here Abhay");
	 				
			handle_physical_makehugemap(current,node1->vm_start,node1->vm_end,prot);
			// // shrink the first
			// first->vm_end = new_start;
				
			// node1->vm_next = node2;	
			// node2->vm_next = first->vm_next;

	 	// 	first->vm_next = node1;		

	 		
	 		
	 		return new_start;
		}


		struct vm_area *node= create_vm_area(new_start,new_end,prot,HUGE_PAGE_MAPPING);
			
		handle_physical_makehugemap(current,node->vm_start,node->vm_end,prot);

		struct vm_area *temp1 = first->vm_next,*temp2;
		
		
		while(temp1!=second){
			temp2=temp1->vm_next;
			dealloc_vm_area(temp1);
			temp1=temp2;
		}

		// shrink the first
		first->vm_end = new_start;

		first->vm_next = node;

		node->vm_next = second ;

		// shrink the second
		second->vm_start = new_end;
		
		return new_start;

	}	


}


/**
 * break_system call implemenation
 */
u64 check_if_physical_allocated_huge(struct exec_context *ctx,u64 addr){

	u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
	u64 *entry;
	u64 pfn;
	
	
	// find the entry in page directory
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	if(*entry & 0x1){

		// pfn = (*entry >> HUGEPAGE_SHIFT) & 0xFFFFFFFF;
		// os_hugepage_free();
		vaddr_base = (u64*)((*entry >> 21) << 21 );
		
		// will be3 used as read_bufer

		return (u64)vaddr_base;

	}
	else{
		return 0;
	}
	//

}

void install_4K_pagetable(struct exec_context *ctx, u64 addr, void* buff, u32 prot ){
	u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
	u64 *entry;
	u64 pfn;
	// set User and Present flags
	// set Write flag if specified in error_code
	u64 ac_flags = 0x5 | (prot & 0x2);
	
	// find the entry in page directory
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PUD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);

	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PMD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	if(*entry & 0x1) {
		// PMD->PTE Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PLD 
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	entry = vaddr_base + ((addr & PTE_MASK) >> PTE_SHIFT);
	
	pfn = ((u64)buff >> PTE_SHIFT) & 0xFFFFFFFF;

	*entry = (pfn << PTE_SHIFT) | ac_flags;	
	
	return ;

}

void handle_physical_breakhuge(struct exec_context *current, u64 start, u64 end,u32 prot){

	for(u64 i = start;i<end;i+=0x200000){
		
		u64 read = check_if_physical_allocated_huge(current,i);
		if(read == 0){
			// not allocated
			// do nothing		
		}
		else{
			// 4K pages 512 in number
			
			void* read_buff = os_hugepage_alloc();
			memcpy((char*)read_buff,(char*)read,0x200000);
			delete_if_physical_allocated_huge(current,i);
			
			for(u64 j = i;j < i+0x200000; j+=0x1000){
				// printk("Abhay");
				u32 pfn = os_pfn_alloc(USER_REG);
				void* write_buff = osmap(pfn);
				
				memcpy((char *)write_buff,(char *)(read_buff+j-i),0x1000);
				// if(j==i){
				// 	printk("%c,%c",(char*)write_buff,(char*)(write_buff+4095));
				// }
				install_4K_pagetable(current,j,write_buff,prot);
			
			}
			
		}
	}
	return ;
}


int vm_area_break_hugepage(struct exec_context *current, void *inaddr, u32 length)
{

	if( inaddr == NULL || (u64 )inaddr % 0x200000 || length % 0x200000 || length <= 0)
		return -EINVAL;


	struct vm_area *iter= current->vm_area;
	
	u64 addr = (u64) inaddr;
	// This time small the code bro!!!


	while(iter!=NULL){
		
		if(iter->mapping_type == NORMAL_PAGE_MAPPING){
			//do nothing
		}
		else{
			
			if(iter->vm_end <= addr || iter->vm_start >= addr + length ){
				// outside the range
				//do nothing
			}
			else if(iter->vm_start >= addr && iter->vm_end <= addr + length ){
				// completely inside the range
				handle_physical_breakhuge(current,iter->vm_start,iter->vm_end,iter->access_flags);
				iter->mapping_type = NORMAL_PAGE_MAPPING;
				right_merge(iter);
				left_merge(current,iter);
				
			}
			else if(iter->vm_start < addr && iter->vm_end <= addr + length){
				
				// simply break the iter in two and next iter will do its work on its own
				struct vm_area *node = create_vm_area(addr,iter->vm_end,iter->access_flags,HUGE_PAGE_MAPPING);
				iter->vm_end = addr;
				node->vm_next = iter->vm_next;
				iter->vm_next = node;
				
				// right_merge(node);
				// now next iter will go in condn 2 !!! YAYYY
			}
			else if(iter->vm_start >= addr && iter->vm_end > addr + length){
				struct vm_area *node = create_vm_area(addr+length,iter->vm_end,iter->access_flags,HUGE_PAGE_MAPPING);
				iter->vm_end = addr+length;

				handle_physical_breakhuge(current,iter->vm_start,iter->vm_end,iter->access_flags);
				
				iter->mapping_type = NORMAL_PAGE_MAPPING;
				node->vm_next = iter->vm_next;
				iter->vm_next = node;
				left_merge(current,iter);
			}

			else if(iter->vm_start < addr && iter->vm_end > addr + length){
				// simply split in 3 HUGE MAPPINGS
				struct vm_area *node1 = create_vm_area(addr,addr+length,iter->access_flags,HUGE_PAGE_MAPPING);
								
				struct vm_area *node2 = create_vm_area(addr+length,iter->vm_end,iter->access_flags,HUGE_PAGE_MAPPING);
				
				// shrink iter
				iter->vm_end = addr;

				node2->vm_next = iter->vm_next;

				node1->vm_next = node2;

				iter->vm_next = node1;			
			}
		}
		// node fully inside
		iter= iter->vm_next;
	}

	return 0;
}
