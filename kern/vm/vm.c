#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

/* Place your page table functions here */

int vm_initPT(paddr_t **pagetable, uint32_t msb, uint32_t ssb) {
    
    /* 1st level of the page table is indexed by 8 most significant bits */
    pagetable[msb] = kmalloc(sizeof(paddr_t *) * PT_LVL2_SIZE);
    
    if (pagetable[msb] == NULL)
        return ENOMEM; /* out of memory */
    
    for (int i = 0; i < PT_LVL2_SIZE; i++) {
        /* lazy allocated so initialise to NULL */
        pagetable[msb][i] = NULL;
    }

    /* 2nd level of the page table indexed by 6 second-most significant bits */
    pagetable[msb][ssb] = kmalloc(sizeof(paddr_t) * PT_LVL3_SIZE);
    
    if (pagetable[msb][ssb] == 0)
        return ENOMEM; /* out of memory */

    for (int i = 0; i < PT_LVL3_SIZE; i++) {
        /* zero-fill */
        pagetable[msb][ssb][i] = 0;
    }

    return 0;
}

int vm_addPTE(paddr_t **pagetable, uint32_t msb, uint32_t ssb, uint32_t lsb) {

    /* ADD PAGE TABLE ENTRY */

    if (pagetable[msb] == NULL)
        vm_initPT(pagetable, msb, ssb);

    /* allocate a kernel heap page */
    vaddr_t kpage = alloc_kpages(1); 

    if (kpage == 0)
        return ENOMEM; /* out of memory */

    /* convert to physical address to use as frame to back virtual page */
    paddr_t frame = KVADDR_TO_PADDR(kpage);

    /* PTE ATTRIBUTES (bits)
     * valid bit - valid mapping for the page (present/absent)
     * dirty bit - write privilege bit; indicates modified in memory 
     */
    
    pagetable[msb][ssb][lsb] = (frame & TLBLO_PPAGE) | TLBLO_VALID | TLBLO_DIRTY;

    return 0;
}


// int vm_freePT(paddr_t **pagetable) {
    
//     /* loop through first level */
//     for (int msb = 0; msb < PT_LVL1_SIZE; msb++) {
        
//         /* loop through second level */
//         for (int ssb = 0; ssb < PT_LVL2_SIZE; ssb++) {

//             /* loop through third level */
//             for (int lsb = 0; lsb < PT_LVL3_SIZE; slb++) {

//                 /* delete frame */
//                 paddr_t paddr = pagetable[msb][ssb][lsb] & TABLO_PPAGE;
//                 vaddr_t kpage = PADDR_TO_KVADDR(paddr);
//                 free_kpage(kpage);

//                 pagetable[msb][ssb][lsb] = NULL;
//             }
//             kfree(pagetable[msb][ssb]);
//         }
//         kfree(pagetable[msb]);
//     }    
//     return 0;
// }


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

