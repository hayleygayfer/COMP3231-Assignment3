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

/////////////////////////////////////////////////////
// HELPER FUNCTIONS FOR ADDING TO PAGE TABLE
/////////////////////////////////////////////////////

/* Helper functions */
uint32_t get_msb (uint32_t addr) {
    /* get most significant 8 bits */
    return addr >> 24;
}

uint32_t get_ssb (uint32_t addr) {
    /* get next 6 bits */
    return addr << 8 >> 26;
}

uint32_t get_lsb (uint32_t addr) {
    /* get least significant 6 bits */
    return addr << 14 >> 26;
}

int vm_init_first_level(paddr_t ***pagetable) {
     
    pagetable = kmalloc(sizeof(paddr_t **) * PT_LVL1_SIZE);

    if (pagetable == NULL) {
        kfree(pagetable);
        return ENOMEM; /* out of memory */
    }

    for (int i = 0; i < PT_LVL1_SIZE; i++) {
        /* lazy allocated so initialise to NULL */
        pagetable[i] = NULL;
    }

    return 0;
}

int vm_init_second_level(paddr_t ***pagetable, uint32_t msb) {

    pagetable[msb] = kmalloc(sizeof(paddr_t *) * PT_LVL2_SIZE);

    if (pagetable[msb] == NULL) {
        kfree(pagetable[msb]);
        return ENOMEM; /* out of memory */
    }

    bzero(pagetable[msb], PT_LVL2_SIZE * sizeof(paddr_t *));

    for (int i = 0; i < PT_LVL2_SIZE; i++) {
        /* lazy allocated so initialise to NULL */
        pagetable[msb][i] = NULL;
    }

    return 0;
}

int vm_init_third_level(paddr_t ***pagetable, uint32_t msb, uint32_t ssb) {

    pagetable[msb][ssb] = kmalloc(sizeof(paddr_t) * PT_LVL3_SIZE);
    
    if (pagetable[msb][ssb] == 0) {
        kfree(pagetable[msb][ssb]);
        return ENOMEM; /* out of memory */
    }
    bzero(pagetable[msb][ssb], PT_LVL3_SIZE * sizeof(paddr_t));

    return 0;
}


/////////////////////////////////////////////////////
// HELPER FUNCTIONS FOR COPYING PAGE TABLE
/////////////////////////////////////////////////////

int vm_copy_entry(paddr_t ***old_pt, paddr_t ***new_pt, int msb, int ssb, int lsb) {

    vaddr_t kpage = alloc_kpages(1);

    if (kpage == 0) {
        free_kpages(kpage);
        return ENOMEM;
    }

    // bzero((void *)kpage, PAGE_SIZE);
    paddr_t old_frame = old_pt[msb][ssb][lsb] & PAGE_FRAME;
    vaddr_t old_page = PADDR_TO_KVADDR(old_frame);

    void *res = memmove((void *)kpage, (const void *)old_page, PAGE_SIZE);

    if (res == NULL) {
        vm_freePT(new_pt);
        return ENOMEM;
    } 
    // TODO: dirty bit
    
    paddr_t new_frame = KVADDR_TO_PADDR(kpage);
    new_pt[msb][ssb][lsb] = (new_frame & PAGE_FRAME) | TLBLO_VALID | TLBLO_DIRTY;

    return 0;
}

int vm_init_copy_second_level(paddr_t ***new_pt, int msb) {

    /* create second level of copy table */
    new_pt[msb] = kmalloc(sizeof(paddr_t *) * PT_LVL2_SIZE);
    if (new_pt[msb] == NULL) {
        kfree(new_pt[msb]);
        return ENOMEM; /* out of memory */
    }

    /* initialise second level of copy table */
    bzero(new_pt[msb], PT_LVL2_SIZE * sizeof(paddr_t *));
    for (int i = 0; i < PT_LVL2_SIZE; i++)
        new_pt[msb][i] = NULL;

    return 0;
}

int vm_init_copy_third_level( paddr_t ***new_pt, int msb, int ssb) {

    /* create third level of the copy table */
    new_pt[msb][ssb] = kmalloc(sizeof(paddr_t) * PT_LVL3_SIZE);
    if (new_pt[msb][ssb] == NULL) {
        kfree(new_pt[msb][ssb]);
        return ENOMEM; /* out of memory */
    }

    /* initialise third level of copy table */
    bzero(new_pt[msb][ssb], PT_LVL3_SIZE * sizeof(paddr_t));

    return 0;
}

/////////////////////////////////////////////////////
//         PAGE TABLE FUNCTIONS
/////////////////////////////////////////////////////

int vm_initPT(paddr_t ***pagetable, vaddr_t faultaddress) {

    paddr_t p_fault = KVADDR_TO_PADDR(faultaddress);

    uint32_t msb = get_msb (p_fault);
    uint32_t ssb = get_ssb (p_fault);

    /* 1st level of the page table is indexed by 8 most significant bits */
    if (pagetable == NULL) {
        int ret1 = vm_init_first_level(pagetable);

        if (ret1)
            return ret1;
    }

    /* 2nd level of the page table indexed by 6 second-most significant bits */
    if (pagetable[msb] == NULL) {
        int ret2 = vm_init_second_level(pagetable, msb);

        if (ret2)
            return ret2;
        
    }

    /* 3rd level of the page table indexed by 6 second-most significant bits */
    if (pagetable[msb][ssb] == NULL) {
        int ret3 = vm_init_third_level(pagetable, msb, ssb);

        if (ret3)
            return ret3;
    }

    return 0;
}


int vm_addPTE(paddr_t ***pagetable, vaddr_t faultaddress) {

    paddr_t p_fault = KVADDR_TO_PADDR(faultaddress);

    uint32_t msb = get_msb(p_fault);
    uint32_t ssb = get_ssb(p_fault);
    uint32_t lsb = get_lsb(p_fault);

    /* ADD PAGE TABLE ENTRY */

    if (pagetable == NULL || pagetable[msb] == NULL || pagetable[msb][ssb] == NULL)
        vm_initPT(pagetable, faultaddress);
    
    /* allocate a kernel heap page */
    vaddr_t kpage = alloc_kpages(1);
    // bzero((void *)kpage, PAGE_SIZE);
    
    if (kpage == 0) {
        free_kpages(kpage);
        return ENOMEM; /* out of memory */
    }

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

    /* no page table to copy */
    if (old_pt == NULL) {
        new_pt = NULL;
        return 0;
    }
    
    /* loop through first level of the page table */
    for (int i = 0; i < PT_LVL1_SIZE; i++) {
        
        if (old_pt[i] == NULL)
            continue;

        /* create and initialise second level */
        int err = vm_init_copy_second_level(new_pt, i);
        if (err)
            return err;

        /* loop through second level of the page table */
        for (int j = 0; j < PT_LVL2_SIZE; j++) {
            
            if (old_pt[i][j] == NULL)
                continue;

            int ret = vm_init_copy_third_level(new_pt, i, j);
            if (ret)
                return ret;

            /* loop through third level of the page table */
            for (int k = 0; k < PT_LVL3_SIZE; k++) {
                
                /* if there's content in the page table, copy over */
                if (old_pt[i][j][k]) {
                    int res = vm_copy_entry(old_pt, new_pt, i, j, k);
                    if (res)
                        return res;
                }
            }
        }
    }
    return 0;
}

int vm_freePT(paddr_t ***pagetable) {
    
    if (pagetable == NULL) {
        return 0;
    }
        
    /* loop through first level */
    for (int msb = 0; msb < PT_LVL1_SIZE; msb++) {
        
        if (pagetable[msb] == NULL) {
            continue;
        }
        
        /* loop through second level */
        for (int ssb = 0; ssb < PT_LVL2_SIZE; ssb++) {

            if (pagetable[msb][ssb] == NULL) {
                continue;
            }

            /* loop through third level */
            for (int lsb = 0; lsb < PT_LVL3_SIZE; lsb++) {
                /* delete frame */
                if (pagetable[msb][ssb][lsb]) {
                    paddr_t paddr = pagetable[msb][ssb][lsb] & PAGE_FRAME;
                    vaddr_t kpage = PADDR_TO_KVADDR(paddr);
                    pagetable[msb][ssb][lsb] = 0;
                    free_kpages(kpage);
                }
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
    if (curproc == NULL) {
        return EFAULT;
    }

    /* write to a read only page was attempted */
    if (faulttype == VM_FAULT_READONLY) {
        return EFAULT;
    }

    /* lookup page table for page table entry */
    paddr_t pte = lookupPTE(curproc->p_addrspace, faultaddress);

    /* check valid transatlion */
    if (pte != 0 && (pte & TLBLO_VALID)) {
        /* load TLB */
        int spl = splhigh();

        uint32_t entry_hi = faultaddress & TLBHI_VPAGE;
        uint32_t entry_lo = (pte & PAGE_FRAME) | TLBLO_VALID | TLBLO_DIRTY;

        tlb_random(entry_hi, entry_lo);
        splx(spl);

        return 0;
    }

    /* look up region */
    region *faultregion = lookup_region(curproc->p_addrspace, faultaddress);

    /* check valid region */
    if (faultregion == NULL) {
       // panic("here %d %p", faultaddress, curproc->p_addrspace);
       return EFAULT;
    }

    /* not writable */
    if ((faulttype == VM_FAULT_WRITE) && ((faultregion->flags & PF_W) == 0)) {
        return EFAULT;
    }

    /* Allocate frame, zerofill, insert PTE */
    if (curproc->p_addrspace == NULL) {
        panic("her3e");
        return EFAULT;
    }

    paddr_t ***pagetable = curproc->p_addrspace->as_pagetable;
    int ret = vm_addPTE(pagetable, faultaddress);

    if (ret) {
        panic("ret");
        return ret;  
    }

    pte = lookupPTE(curproc->p_addrspace, faultaddress);

    if (pte != 0 && (pte & TLBLO_VALID)) {
        /* load TLB */
        int spl = splhigh();

        uint32_t entry_hi = faultaddress & TLBHI_VPAGE;
        uint32_t entry_lo = (pte & PAGE_FRAME) | TLBLO_VALID | TLBLO_DIRTY;

        tlb_random(entry_hi, entry_lo);
        splx(spl);

        return 0;
    }

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

