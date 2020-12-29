#include<ulib.h>

// Page fault handler working correctly.

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{

	int pageSize = 4096;
	u32 MB = 1 << 20;

    u64 aligned = 0x180400000; // 2MB aligned address

	char *paddr = mmap((void *)aligned, 2*MB, PROT_READ|PROT_WRITE, 0);

	for (int i = 0; i < pageSize; i++) {
		paddr[i] = 'Z'; 
	}

	pmap(1); 

	char *hpaddr = (char *) make_hugepage(paddr, 2*MB , PROT_READ|PROT_WRITE, 0);
	pmap(1);


	for (int i = 0; i < pageSize; i++) {
		hpaddr[i] = 'X'; 
	}

	int diff = 0;
	for (int i = 0; i < pageSize; i++) {
		if (hpaddr[i] != 'X') {
			++diff;	
		}
	}
	
	if(diff)
		printf("FAILED\n");
	else
		printf("PASSED\n");
	
	int ret = break_hugepage(paddr , 2*MB);

	pmap(1);
	if(ret < 0) printf("FAILED BREAK\n");
	else printf("PASSED BREAK\n");
	diff = 0;
	for(int i=0; i<pageSize; ++i){
		if(hpaddr[i] != 'X') ++diff;
	}
	if(diff) printf("FAILED MEMCOPY BREAK\n");
	else printf("PASSED MEMCOPY BREAK\n");
	printf("%x\n",diff);
    int ret2 = munmap(paddr, 2*MB);
 
}
/*
########### 	VM Area Details 	################
	VM_Area:[1]		MMAP_Page_Faults[1]
	[0x180400000	0x180600000] #PAGES[512]	R W _

	###############################################



	########### 	VM Area Details 	################
	VM_Area:[1]		MMAP_Page_Faults[1]
	[0x180400000	0x180600000] #HUGE PAGES[1]	R W _

	###############################################

PASSED
PASSED BREAK
PASSED MEMCOPY BREAK

*/
