#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

/* Place your page table functions here */

int vm_initPT(paddr_t **oldPTE, uint32_t msb) {
    return 0;
}

int vm_addPTE(paddr_t **oldPTE, uint32_t msb, uint32_t ssb, uint32_t lsb) {
    return 0;
}

int vm_freePTE(paddr_t **oldPTE) {
    return 0;
}

int vm_copyPTE(paddr_t **oldPTE, paddr_t **newPTE) {
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

