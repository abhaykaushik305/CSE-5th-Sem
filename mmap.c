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

	
	/// code taken from entry.c > install page fault

	// get base addr of pgdir
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
	// since this fault occured as frame was not present, we don't need present check here
	pfn = os_pfn_alloc(USER_REG);
	*entry = (pfn << PTE_SHIFT) | ac_flags;

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
	if(flags == MAP_FIXED && (addr == NULL || addr % 0x1000)){
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
		if(addr!=NULL && addr % 4096){
			addr = (addr/4096 + 1) * 4096 ; 
		}

		struct vm_area* iter = current->vm_area;

		int myflag=0; // 

		if(addr!=NULL ){

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
					struct vm_area* node = create_vm_area(addr+length,iter->vm_end,iter->access_flags,NORMAL_PAGE_MAPPING);
					iter->vm_end = addr;
					iter->vm_next = node;
					return 0;
				}
				else{
					//shrink
					iter->vm_end = addr; 
					return 0;
				}

			}
			else{
				// HUGE PAGE MAPPING

				// corresponding start and ends
				u64 pred_start = iter->vm_start + ((addr - iter->vm_start)/0x200000)*0x200000 ;

				u64 pred_end;
				if( (addr+length - iter->vm_start) % 0x200000)
					pred_end = iter->vm_start + ((addr+length - iter->vm_start)/0x200000 + 1)*0x200000;

				else 
					pred_end = iter->vm_start + ((addr+length - iter->vm_start)/0x200000)*0x200000 ;

				// find prev node
				struct vm_area* iter2= current->vm_area;
				while(iter2->vm_next != iter){
					iter2 = iter2->vm_next; 
				}


				if(pred_end >= iter->vm_end){
					
					if(pred_start == iter->vm_start){
						// dealloc the last node
						iter2->vm_next = NULL;
						dealloc_vm_area(iter);
						return 0;
					}
					else{
						//shrink left side
						iter->vm_end = pred_start ;
						return 0;
					}
				}
				else{

					if(pred_start == iter->vm_start){
						// shrink right side
						iter->vm_start = pred_end;
						return 0;
					}
					else{
						// split
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
						while(temp1!=iter2){
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}
						iter->vm_next=iter2->vm_next;
						dealloc_vm_area(iter2);
						return 0;
					}
					else{
						// iter2 vm area going to split

						

						struct vm_area* temp1 = iter->vm_next,*temp2;
						while(temp1!=iter2){
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}
						// shrink iter2
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
						while(temp1!=iter2){
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}
						iter->vm_next = iter2->vm_next;
						dealloc_vm_area(iter2);
					}
					else{
						// iter2 vm area going to split
						
						struct vm_area* temp1 = iter->vm_next,*temp2 ;
						while(temp1!=iter2){
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}
						// shrink iter2
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
					temp2=temp1->vm_next;
					dealloc_vm_area(temp1);
					temp1=temp2;
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
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}

						return 0;
						
					}

					// iter will split
					struct vm_area* temp1 = iter->vm_next,*temp2 = iter->vm_next;
					while(temp1!=iter2){
						temp2=temp1->vm_next;
						dealloc_vm_area(temp1);
						temp1=temp2;
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
						struct vm_area* prev_iter= current->vm_area;
						while(prev_iter->vm_next != iter){
							prev_iter = prev_iter -> vm_next;
						}

						
						// iter and iter2 can be same
						if(iter == iter2){
							prev_iter->vm_next = iter2->vm_next;
							dealloc_vm_area(iter);
							return 0;
						}	
						struct vm_area* temp1 = iter,*temp2;
						while(temp1!=iter2){
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}
						prev_iter->vm_next = iter2->vm_next;
						dealloc_vm_area(iter2);
						return 0;
					}

					
					struct vm_area* temp1 = iter->vm_next,*temp2;
					while(temp1!=iter2){
						temp2=temp1->vm_next;
						dealloc_vm_area(temp1);
						temp1=temp2;
					}
					
					// 1 will shrink and 2 gets deleted
					iter->vm_end = pred_start;

					//iter and iter2 can be same 
					if(iter!=iter2){
						iter->vm_next = iter2->vm_next;
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
							iter2->vm_start = pred_end;
							return 0;
						}

						// we need to find previous of iter
						struct vm_area* prev_iter= current->vm_area;
						while(prev_iter->vm_next != iter){
							prev_iter = prev_iter -> vm_next;
						}

						
						
						struct vm_area* temp1 = iter,*temp2;
						while(temp1!=iter2){
							temp2=temp1->vm_next;
							dealloc_vm_area(temp1);
							temp1=temp2;
						}

						iter2->vm_start = pred_end;
						prev_iter->vm_next = iter2; 
						return 0;
					}

					
					if(iter == iter2){

						// there will be split bro 

						struct vm_area *node = create_vm_area(pred_end,iter->vm_end,iter->access_flags,iter->mapping_type);
						iter->vm_end = pred_start;
						node->vm_next = iter->vm_next;
						iter->vm_next  = node;
						return 0;
					}
					
					
					struct vm_area* temp1 = iter->vm_next,*temp2 = iter->vm_next;
					while(temp1!=iter2){
						temp2=temp1->vm_next;
						dealloc_vm_area(temp1);
						temp1=temp2;
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

		struct vm_area *temp1 = first->vm_next,*temp2;
		
		first->vm_next = second->vm_next;
		
		while(temp1!=second->vm_next){
			temp2=temp1->vm_next;
			dealloc_vm_area(temp1);
			temp1=temp2;
		}

		// expand the first
		first->mapping_type = HUGE_PAGE_MAPPING;
		first->access_flags = prot;
		first->vm_end = new_end;
		
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
	 		left_merge(current,first);	 
	 		return new_start;
		}


		struct vm_area *temp1 = first->vm_next,*temp2;
		
		first->vm_next = second;
		
		while(temp1!=second){
			temp2=temp1->vm_next;
			dealloc_vm_area(temp1);
			temp1=temp2;
		}

		// expand the first
		first->mapping_type = HUGE_PAGE_MAPPING;
		first->access_flags = prot;
		first->vm_end = new_end;

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
	 		right_merge(node);
	 		return new_start;
		}


		struct vm_area *temp1 = first->vm_next,*temp2;
		
		first->vm_next = second;
		
		while(temp1!=second){
			temp2=temp1->vm_next;
			dealloc_vm_area(temp1);
			temp1=temp2;
		}

		// shrink the first
		first->vm_end = new_start;

		// expand the second
		second->vm_start = new_start;
		second->mapping_type = HUGE_PAGE_MAPPING;
		second->access_flags = prot;
		right_merge(second);
		return new_start;

	}

	
	if(first->vm_start != new_start && second->vm_end != new_end){

		if(first == second){
			// no deletion
			// 2 new node created
			struct vm_area *node1= create_vm_area(new_start,new_end,prot,HUGE_PAGE_MAPPING);
			struct vm_area *node2= create_vm_area(new_end,first->vm_end,first->access_flags,NORMAL_PAGE_MAPPING);
			
			// shrink the first
			first->vm_end = new_start;
				
			node1->vm_next = node2;	
			node2->vm_next = first->vm_next;

	 		first->vm_next = node1;		

	 		return new_start;
		}


		struct vm_area *node= create_vm_area(new_start,new_end,prot,HUGE_PAGE_MAPPING);
			

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
int vm_area_break_hugepage(struct exec_context *current, void *inaddr, u32 length)
{

	if( inaddr == NULL || (u64 )inaddr % 0x200000 || length % 0x200000)
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
				iter->mapping_type = NORMAL_PAGE_MAPPING;
				right_merge(iter);
				left_merge(current,iter);
				
			}
			else if(iter->vm_start < addr && iter->vm_end <= addr + length){
				struct vm_area *node = create_vm_area(addr,iter->vm_end,iter->access_flags,HUGE_PAGE_MAPPING);
				iter->vm_end = addr;
				node->vm_next = iter->vm_next;
				iter->vm_next = node;
				right_merge(node);
				// now next iter will go in condn 2 !!! YAYYY
			}
			else if(iter->vm_start >= addr && iter->vm_end > addr + length){
				struct vm_area *node = create_vm_area(addr+length,iter->vm_end,iter->access_flags,HUGE_PAGE_MAPPING);
				iter->vm_end = addr+length;
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
