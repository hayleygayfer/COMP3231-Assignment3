#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

#include <proc.h>
#include <spl.h>
#include <elf.h>
#include <current.h>

/* Helper functions */
uint32_t get_msb (uint32_t addr) {
    /* get most significant 8 bits */
    return addr >> 24;
}

uint32_t get_ssb (uint32_t addr) {
    /* get next 6 bits */
    uint32_t mask = 0x3f;
    return (addr >> 14) & mask;
}

uint32_t get_lsb (uint32_t addr) {
    /* get last 6 bits */
    uint32_t mask = 0x3f;
    return (addr & mask);
}

/* Place your page table functions here */

int vm_initPT(paddr_t ***pagetable, vaddr_t faultaddress) {

    uint32_t msb = get_msb (faultaddress);
    uint32_t ssb = get_ssb (faultaddress);

    /* 1st level of the page table is indexed by 8 most significant bits */
    pagetable = kmalloc(sizeof(paddr_t **) * PT_LVL1_SIZE);

    if (pagetable == NULL)
        return ENOMEM; /* out of memory */

    for (int i = 0; i < PT_LVL1_SIZE; i++) {
        /* lazy allocated so initialise to NULL */
        pagetable[i] = NULL;
    }

    /* 2nd level of the page table indexed by 6 second-most significant bits */
    pagetable[msb] = kmalloc(sizeof(paddr_t *) * PT_LVL2_SIZE);

    if (pagetable[msb] == NULL)
        return ENOMEM; /* out of memory */

    for (int i = 0; i < PT_LVL2_SIZE; i++) {
        /* lazy allocated so initialise to NULL */
        pagetable[msb][i] = NULL;
    }

    /* 3rd level of the page table indexed by 6 second-most significant bits */
    pagetable[msb][ssb] = kmalloc(sizeof(paddr_t) * PT_LVL3_SIZE);
    
    if (pagetable[msb][ssb] == 0)
        return ENOMEM; /* out of memory */

    for (int i = 0; i < PT_LVL3_SIZE; i++) {
        /* zero-fill */
        pagetable[msb][ssb][i] = 0;
    }

    return 0;
}

int vm_addPTE(paddr_t ***pagetable, vaddr_t faultaddress) {

    uint32_t msb = get_msb (faultaddress);
    uint32_t ssb = get_ssb (faultaddress);
    uint32_t lsb = get_lsb (faultaddress);

    /* ADD PAGE TABLE ENTRY */

    if (pagetable[msb] == NULL)
        vm_initPT(pagetable, faultaddress);

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
    
    pagetable[msb][ssb][lsb] = (frame & PAGE_FRAME) | TLBLO_VALID | TLBLO_DIRTY;

    return 0;
}

int vm_copyPTE(paddr_t ***old_pt, paddr_t ***new_pt) {
    uint32_t dirty = 0;

    // Loop through 1st level entries
    for (int i = 0; i < PT_LVL1_SIZE; i++) {
        // check if populated
        if (old_pt[i] == NULL) continue;

        /* 2nd level of the page table indexed by 6 second-most significant bits */
        new_pt[i] = kmalloc(sizeof(paddr_t) * PT_LVL2_SIZE);

        for (int j = 0; j < PT_LVL2_SIZE; j++) {

            if (old_pt[i][j] == NULL) continue;

            /* 3rd level of the page table indexed by 6 second-most significant bits */
            new_pt[i][j] = kmalloc(sizeof(paddr_t) * PT_LVL3_SIZE);

            for (int k = 0; k < PT_LVL3_SIZE; k++) {
                if (old_pt[i][j][k] == 0) {
                    // zero fill 3rd level index
                    new_pt[i][j][k] = 0;
                } else {
                    // If not zero filled (has content set)
                    vaddr_t n_frame = alloc_kpages(1);
                    // check if no available memory
                    if (n_frame == 0) return ENOMEM;
                    bzero((void *)n_frame, PAGE_SIZE);
                    // copy bytes
                    if (memmove((void *)n_frame, (const void *)PADDR_TO_KVADDR(old_pt[i][j][k] & PAGE_FRAME), PAGE_SIZE) == NULL) { 
                        // if memmove FAILS
                        vm_freePT(new_pt);
                        return ENOMEM; // Out of memory
                    }
                    dirty = old_pt[i][j][k] & TLBLO_DIRTY;
                    new_pt[i][j][k] = (KVADDR_TO_PADDR(n_frame) & PAGE_FRAME) | dirty | TLBLO_VALID;
                }
            }
        }
    }
    return 0;
}

int vm_freePT(paddr_t ***pagetable) {
    
    /* loop through first level */
    for (int msb = 0; msb < PT_LVL1_SIZE; msb++) {

        if (pagetable[msb] == NULL)
            break;
        
        /* loop through second level */
        for (int ssb = 0; ssb < PT_LVL2_SIZE; ssb++) {

            if (pagetable[msb][ssb] == NULL)
                break;

            /* loop through third level */
            for (int lsb = 0; lsb < PT_LVL3_SIZE; lsb++) {

                /* delete frame */
                paddr_t paddr = pagetable[msb][ssb][lsb] & PAGE_FRAME;
                vaddr_t kpage = PADDR_TO_KVADDR(paddr);
                free_kpages(kpage);

                pagetable[msb][ssb][lsb] = 0;
            }
            kfree(pagetable[msb][ssb]);
        }
        kfree(pagetable[msb]);
    }    
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

int vm_fault(int faulttype, vaddr_t faultaddress) {

    /* Given a virtual address, find physical address and put inside TLB */

    /* write to a read only page was attempted */
    if (faulttype == VM_FAULT_READONLY)
        return EFAULT;
    
    /* lookup page table for page table entry */
    paddr_t pte = lookupPTE(curproc->p_addrspace, faultaddress);

    /* check valid transatlion */
    if (pte != 0 && (pte & TLBLO_VALID)) {
        /* load TLB */
        int spl = splhigh();

        uint32_t entry_hi = faultaddress;
        uint32_t entry_lo = (pte & PAGE_FRAME) | TLBLO_VALID | TLBLO_DIRTY;

        tlb_random(entry_hi, entry_lo);

        splx(spl);

    }

    /* look up region */
    region *faultregion = lookup_region(curproc->p_addrspace, faultaddress);

    /* check valid region */
    if (faultregion == NULL)
        return EFAULT;
    
    /* not writable */
    if ((faulttype == VM_FAULT_WRITE) && ((faultregion->flags & PF_W) == 0))
        return EFAULT;

    /* Allocate frame, zerofill, insert PTE */

    paddr_t ***pagetable = curproc->p_addrspace->as_pagetable;

    int ret = vm_addPTE(pagetable, faultaddress);

    if (ret)
        return ret;  

    return 0;
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

