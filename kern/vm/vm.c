#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

/* Place your page table functions here */

int vm_initPT(paddr_t **pagetable, uint32_t msb) {

    /* 1st level of the page table is indexed by 8 most significant bits */
    pagetable[msb] = kmalloc(sizeof(paddr_t) * PT_LVL1_SIZE);
    
    if (pagetable[msb] == NULL)
        return ENOMEM; /* out of memory */
    
    for (int i = 0; i < PT_LVL1_SIZE; i++) {
        /* lazy allocated, so initialise to NULL */
        pagetable[msb][i] = NULL;
    }
    
    return 0;
}

int vm_addPTE(paddr_t **pagetable, uint32_t msb, uint32_t ssb, uint32_t lsb) {
    
    /* adding a page table entry into a page table */

    if (pagetable[msb] == NULL) {
        /* no valid entry yet so allocate 1st level of page table*/
        vm_initPT(pagetable, msb);
    }

    /* 2nd level of the page table indexed by 6 second-most significant bits */
    pagetable[msb][ssb] = kmalloc(sizeof(paddr_t) * PT_LVL2_SIZE);

    for (int i = 0; i < PT_LVL1_SIZE; i++) {
        /* zero-fill newly allocated frames */
        pagetable[msb][ssb][i] = 0;
    }

    /****** ADD PAGE TABLE ENTRY ******/

    /* allocate a kernel heap page */
    vaddr_t kpage = alloc_kpages(1); 

    if (kpage == 0)
        return ENOMEM; /* out of memory */


    pagetable[msb][ssb][lsb] = 

    return 0;
}

int vm_freePTE(paddr_t **pagetable) {
    //ada
    return 0;
}

int vm_copyPTE(paddr_t **old_pagetable, paddr_t **new_pagetable) {
    //ada
    return 0;
}

void vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
     *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    (void) faulttype;
    (void) faultaddress;

    panic("vm_fault hasn't been written yet\n");

    return EFAULT;
}

/*
 * SMP-specific functions.  Unused in our UNSW configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

