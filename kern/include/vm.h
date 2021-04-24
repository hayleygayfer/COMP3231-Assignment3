/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VM_H_
#define _VM_H_

/*
 * VM system-related definitions.
 *
 * You'll probably want to add stuff here.
 */

#include <machine/vm.h>

/* page table level sizes */
#define PT_LVL1_SIZE 256
#define PT_LVL2_SIZE 64
#define PT_LVL3_SIZE 64

/* Fault-type arguments to vm_fault() */

#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

/*** Helper functions ***/

/* get most significant 8 bits */
uint32_t get_msb (uint32_t addr);

/* get next 6 bits */
uint32_t get_ssb (uint32_t addr);

/* get last 6 bits */
uint32_t get_lsb (uint32_t addr);

/*** PTE functions ***/

/* initialise page table and zero-fill */
int vm_initPT(paddr_t ***pagetable, vaddr_t faultaddress);
int vm_init_first_level(paddr_t ***pagetable);
int vm_init_second_level(paddr_t ***pagetable, uint32_t msb);
int vm_init_third_level(paddr_t ***pagetable, uint32_t msb, uint32_t ssb);

/* add page table entry to page table */
int vm_addPTE(paddr_t ***pagetable, vaddr_t faultaddress);

/* copy page table into new address */
int vm_copyPTE(paddr_t ***old_pt, paddr_t ***new_pt);

/* free page table */
int vm_freePT(paddr_t ***pagetable);

/* Initialization function */
void vm_bootstrap(void);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);

/* TLB shootdown handlingpaddr_t vm_lookupPTE(struct addrspace *as, vaddr_t faultaddress) {

    paddr_t ***pagetable = as->as_pagetable;

    uint32_t msb = get_msb (faultaddress);
    uint32_t ssb = get_ssb (faultaddress);
    uint32_t lsb = get_lsb (faultaddress);

    if (pagetable[msb] == 0)
        return 0;

    paddr_t page_table_entry = pagetable[msb][ssb][lsb];

    if (page_table_entry)
        return page_table_entry;

    return 0;
} called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown *);


#endif /* _VM_H_ */
