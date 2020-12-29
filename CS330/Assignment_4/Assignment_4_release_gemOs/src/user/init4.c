#include<ulib.h>

// munmap with huge pages

/*


	########### 	VM Area Details 	################
	VM_Area:[1]		MMAP_Page_Faults[1]
	[0x180400000	0x180A00000] #PAGES[1536]	R _ _

	###############################################

VAL = 1


	########### 	VM Area Details 	################
	VM_Area:[1]		MMAP_Page_Faults[1]
	[0x180400000	0x180A00000] #HUGE PAGES[3]	R W _

	###############################################

PASSED


	########### 	VM Area Details 	################
	VM_Area:[2]		MMAP_Page_Faults[1]
	[0x180400000	0x180600000] #PAGES[512]	R W _
	[0x180600000	0x180A00000] #HUGE PAGES[2]	R W _

	###############################################


*/


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
	u32 MB = 1 << 20;
    u32 kb = 0x1000;
    u64 aligned = 0x180400000; // 2MB aligned address
	
    char* paddr = mmap(aligned, 6*MB, PROT_READ, 0);
    // char* h = mmap((void*)paddr+kb, kb, 3, 0);
    char b = paddr[0];
    pmap(1); // vm area 1
    // // paddr[0] = 'c';
    // char* h = mmap(aligned, 2*MB, 1, 0);
    // pmap(1);
    // char* g = mmap(aligned, 3*MB, 3, 0);
    // // paddr[0] = 'a';
    char* p = make_hugepage((void*)paddr, 6*MB, 3, 1);
    // p = make_hugepage(g, 2*MB, 3, 1);
    // if(p == NULL) printf("NULL HAI\n");
    // printf("ADDR %x\n", p);
    // paddr[0] = 'b';
    // paddr[kb] = 'c';
    // h[0] = 'c';
    p[0] = '1';
    printf("VAL = %c\n", p[0]);
    pmap(1);

    // munmap((void*)paddr+2*MB + 4*kb, 2*kb+1);
    break_hugepage((void*)paddr, 2*MB);
    if(p[0] != '1') printf("FAILED\n");
    else printf("PASSED\n");
    // pmap()
    pmap(1);
}
