#include<ulib.h>



int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{

	int pageSize = 4096;
	u32 MB = 1 << 20;
	// allocate 8M region
	char *addr = mmap(NULL, 8*MB, PROT_READ|PROT_WRITE, 0);
	pmap(1);
	printf("mmap start_addr = %x\n", addr); // addr is not 2MB aligned
	
	// find next 2M aligned 
	u64 aligned = (u64)addr;
	// if(aligned & 0x1fffff){
	// 	aligned >>= 21;
	// 	++aligned;
	// 	aligned <<= 21;
	// }
	
	int printing = 50;
	for (int i = 0; i < 8*MB/1024 ; i++) { // Number of page faults = 2048
		*(((char *)aligned) + i*1024) = 'A' + (i)%26;
		if(printing > 0) printf("%c ", 'A'+(i)%26);
		printing--;
	}
	printf("I am out\n\n");
    char *addr_aligned = (char *) make_hugepage(addr, 8*MB , PROT_READ|PROT_WRITE, 0);
    /*
	  Find next huge page boundary address (aligned)
      Make as many huge pages as possible in the address range (aligned, addr+8M)
      The starting of the huge page should be from a 2MB aligned address, aligned
    */

    pmap(1); //2048

    int diff = 0;
	printing = 50;
	for (int i = 0; i < 8*MB/1024; i++) {
		if(printing > 0) printf("%c ", addr[i*1024]);
		printing--;
        // The content of the pages that get converted to huge pages are preserved.
		if (addr[i*1024] != ('A'+ ((i)%26))) {
			++diff;	
		}
	}

	pmap(1); //2048
	printf("%d\n", diff);
    if(diff)
		printf("Test failed\n");
	else
		printf("Test passed\n");
    

    break_hugepage(addr_aligned, 4*MB);
    pmap(1);

    diff = 0;
	printing = 50;
	for (int i = 0; i < 8*MB/1024; i++) {
		if(printing > 0) printf("%c ", addr[i*1024]);
		printing--;
        // The content of the pages that get converted to huge pages are preserved.
		if (addr[i*1024] != ('A'+ ((i)%26))) {
			++diff;	
		}
	}
    printf("\n\n");
    // printf("diff = %x\n", diff);
	if(diff)
		printf("Test failed\n");
	else
		printf("Test passed\n");
    
    pmap(1);

    munmap(addr,1*MB);
    pmap(1);

    diff = 0;
	printing = 50;
	for (int i = (1*MB/1024 + 1); i < 8*MB/1024; i++) {
		if(printing > 0) printf("%c ", addr[i*1024]);
		printing--;
        // The content of the pages that get converted to huge pages are preserved.
		if (addr[i*1024] != ('A'+ ((i)%26))) {
			++diff;	
		}
	}
    printf("\n\n");
    // printf("diff = %x\n", diff);
	if(diff)
		printf("Test failed\n");
	else
		printf("Test passed\n");
    
    pmap(1);

    munmap(addr+2*MB,2*MB);
    pmap(1);

    diff = 0;
	printing = 50;
	for (int i = (1*MB/1024 + 1); i < 2*MB/1024; i++) {
		if(printing > 0) printf("%c ", addr[i*1024]);
		printing--;
        // The content of the pages that get converted to huge pages are preserved.
		if (addr[i*1024] != ('A'+ ((i)%26))) {
			++diff;	
		}
	}
    printing = 50;
	for (int i = (4*MB/1024 + 1); i < 8*MB/1024; i++) {
		if(printing > 0) printf("%c ", addr[i*1024]);
		printing--;
        // The content of the pages that get converted to huge pages are preserved.
		if (addr[i*1024] != ('A'+ ((i)%26))) {
			++diff;	
		}
	}
    printf("\n\n");
    // printf("diff = %x\n", diff);
	if(diff)
		printf("Test failed\n");
	else
		printf("Test passed\n");
    
    pmap(1);

    
}


/*

########### 	VM Area Details 	################
	VM_Area:[1]		MMAP_Page_Faults[0]
	[0x180201000	0x180A01000] #PAGES[2048]	R W _

	###############################################

mmap start_addr = 0x180201000
A B C D E F G H I J K L M N O P Q R S T U V W X Y Z A B C D E F G H I J K L M N O P Q R S T U V W X 



	########### 	VM Area Details 	################
	VM_Area:[3]		MMAP_Page_Faults[2048]
	[0x180201000	0x180400000] #PAGES[511]	R W _
	[0x180400000	0x180A00000] #HUGE PAGES[3]	R W _
	[0x180A00000	0x180A01000] #PAGES[1]	R W _

	###############################################

A B C D E F G H I J K L M N O P Q R S T U V W X Y Z A B C D E F G H I J K L M N O P Q R S T U V W X 

	########### 	VM Area Details 	################
	VM_Area:[3]		MMAP_Page_Faults[2048]
	[0x180201000	0x180400000] #PAGES[511]	R W _
	[0x180400000	0x180A00000] #HUGE PAGES[3]	R W _
	[0x180A00000	0x180A01000] #PAGES[1]	R W _

	###############################################

0
Test passed


	########### 	VM Area Details 	################
	VM_Area:[3]		MMAP_Page_Faults[2048]
	[0x180201000	0x180800000] #PAGES[1535]	R W _
	[0x180800000	0x180A00000] #HUGE PAGES[1]	R W _
	[0x180A00000	0x180A01000] #PAGES[1]	R W _

	###############################################

A B C D E F G H I J K L M N O P Q R S T U V W X Y Z A B C D E F G H I J K L M N O P Q R S T U V W X 

Test passed


	########### 	VM Area Details 	################
	VM_Area:[3]		MMAP_Page_Faults[2048]
	[0x180201000	0x180800000] #PAGES[1535]	R W _
	[0x180800000	0x180A00000] #HUGE PAGES[1]	R W _
	[0x180A00000	0x180A01000] #PAGES[1]	R W _

	###############################################



	########### 	VM Area Details 	################
	VM_Area:[3]		MMAP_Page_Faults[2048]
	[0x180301000	0x180800000] #PAGES[1279]	R W _
	[0x180800000	0x180A00000] #HUGE PAGES[1]	R W _
	[0x180A00000	0x180A01000] #PAGES[1]	R W _

	###############################################

L M N O P Q R S T U V W X Y Z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z A B C D E F G H I 

Test passed


	########### 	VM Area Details 	################
	VM_Area:[3]		MMAP_Page_Faults[2048]
	[0x180301000	0x180800000] #PAGES[1279]	R W _
	[0x180800000	0x180A00000] #HUGE PAGES[1]	R W _
	[0x180A00000	0x180A01000] #PAGES[1]	R W _

	###############################################



	########### 	VM Area Details 	################
	VM_Area:[4]		MMAP_Page_Faults[2048]
	[0x180301000	0x180401000] #PAGES[256]	R W _
	[0x180601000	0x180800000] #PAGES[511]	R W _
	[0x180800000	0x180A00000] #HUGE PAGES[1]	R W _
	[0x180A00000	0x180A01000] #PAGES[1]	R W _

	###############################################

L M N O P Q R S T U V W X Y Z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z A B C D E F G H I P Q R S T U V W X Y Z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z A B C D E F G H I J K L M 

Test passed


	########### 	VM Area Details 	################
	VM_Area:[4]		MMAP_Page_Faults[2048]
	[0x180301000	0x180401000] #PAGES[256]	R W _
	[0x180601000	0x180800000] #PAGES[511]	R W _
	[0x180800000	0x180A00000] #HUGE PAGES[1]	R W _
	[0x180A00000	0x180A01000] #PAGES[1]	R W _

	###############################################




########### 	VM Area Details 	################
	VM_Area:[1]		MMAP_Page_Faults[0]
	[0x180600000	0x181000000] #PAGES[2560]	R W _

	###############################################



	########### 	VM Area Details 	################
	VM_Area:[2]		MMAP_Page_Faults[1]
	[0x180400000	0x180600000] #PAGES[512]	R W _
	[0x180600000	0x181000000] #HUGE PAGES[5]	R W _

	###############################################

SECOND STR abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkl


	########### 	VM Area Details 	################
	VM_Area:[1]		MMAP_Page_Faults[1]
	[0x180400000	0x180600000] #PAGES[512]	R W _

	###############################################

*/
