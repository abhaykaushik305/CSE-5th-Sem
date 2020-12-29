long vm_area_make_hugepage(struct exec_context *current, void *v_addr, u32 length, u32 prot, u32 force_prot)
{
        struct vm_area *temp = current->vm_area;
        struct vm_area *front, *front_prev, *last;
        u64 addr = (u64) v_addr;
        u64 end = addr + length;
//      printk("Inside hugepage map\n");
        if(addr < 0 || length < 0)
                return -EINVAL;
        while(temp != NULL){//first check for errors
//              printk("Y\n");
                //consider a node for checking if any part of it overlaps with the area to be huge-paged
                if(temp->vm_start <= end && temp->vm_end >= addr){    
                        //check if there is any unpmapped region
                        if(temp->vm_next != NULL && temp->vm_next->vm_end <= end && temp->vm_next->vm_start != temp->vm_end){
                                //if areas are not contiguous means unmapped in between
                                return -ENOMAPPING;
                        }
                        if(temp->mapping_type == HUGE_PAGE_MAPPING){//already a huge page exist
                                return -EVMAOCCUPIED;
                        }
                        if(force_prot == 0 && temp->access_flags != prot){//if the protection is not forced and a node has different protection
                                return -EDIFFPROT;
                        }
                }
                temp = temp->vm_next;
        }
        if(addr % 0x100000 != 0)
                addr = addr + (0x100000 - addr % 0x100000);
        if(length % 0x100000 != 0)
                length = length + (0x100000 - length % 0x100000) - 0x100000;
        end = addr + length; 
        temp = current->vm_area;
        while(temp != NULL) {//find the nodes corresponding to the start of the HUGE VM and end of it
                struct vm_area *next = temp->vm_next;
                if(next != NULL && next->vm_start == addr){
                        front_prev = temp;
                        front = next;
                }
                else if(next != NULL && next->vm_start < addr && next->vm_end > addr && next->mapping_type == NORMAL_PAGE_MAPPING){
                //if the passed address lies in between some preallcoated vm area which is a normal page split it
                        struct vm_area *temp1 = create_vm_area(addr, next->vm_end, next->access_flags, next->mapping_type);
                        next->vm_end = addr;
                        temp1->vm_next = next->vm_next;
                        next->vm_next = temp;
                        front_prev = next;
                        front = temp1;
                }
                if(temp->vm_end == end)
                        last = temp;
                else if(temp->vm_start < end && temp->vm_end > end && temp->mapping_type == NORMAL_PAGE_MAPPING){
                //if the ending address lies in between some normal page split it
                        struct vm_area *temp1 = create_vm_area(end, temp->vm_end, temp->access_flags, temp->mapping_type);
                        temp->vm_end = end;
                        temp1->vm_next = temp->vm_next;
                        temp->vm_next = temp1;
                        last = temp;
                }
                temp = next;
//              printk("X\n");
        }
//      printk("Front start:%x, Last end:%x\n", front->vm_start, last->vm_end);
        temp = front;
        struct vm_area *huge_new = create_vm_area(front->vm_start, last->vm_end, prot, HUGE_PAGE_MAPPING);
//      printk("Huge Start: %x, End: %x\n", huge_new->vm_start, huge_new->vm_end);
//      if(huge_new->vm_end == last->vm_end){//if the huge page can be created at the desired location
//              printk("Z\n");
                front_prev->vm_next = huge_new;
                huge_new->vm_next = last->vm_next;//put the huge page node in place
                temp = front;
                while(temp != last->vm_next){//deallocate all the currently existing nodes
                        dealloc_vm_area(temp);
                        temp = temp->vm_next;
                }
//      }
//      else return -1;
        struct vm_area *next = huge_new->vm_next;
        if(next != NULL && next->mapping_type == HUGE_PAGE_MAPPING && next->vm_start == huge_new->vm_end){
                huge_new->vm_next = next->vm_next;
                huge_new->vm_end = next->vm_end;
                dealloc_vm_area(huge_new);
        }
        if(front_prev->mapping_type == HUGE_PAGE_MAPPING && front_prev->vm_end == huge_new->vm_start){
                front_prev->vm_next = huge_new->vm_next;
                front_prev->vm_end = huge_new->vm_end;
                dealloc_vm_area(huge_page);
        }
        return huge_new->vm_start;
}