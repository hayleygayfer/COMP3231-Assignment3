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

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <elf.h>

#define STACK_PAGES		16
#define STACK_MEMSIZE	STACK_PAGES * PAGE_SIZE

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));

	if (as == NULL) {
		kfree(as);
		return NULL; /* ENOMEM */
	}

	/* no regions initially*/
	as->as_stack = USERSTACK;
	as->as_regions = NULL;

	/* Initialise 3 Level Page Table */ 
	as->as_pagetable = kmalloc(sizeof(paddr_t **) * PT_LVL1_SIZE);	
	
	if (as->as_pagetable == NULL) {
		kfree(as);
		return NULL; /* ENOMEM */
	}

	for (int i = 0; i < PT_LVL1_SIZE; i++) 
		as->as_pagetable[i] = 0;

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	/* create a new address space to populate */
	struct addrspace *newas = as_create();

	if (newas == NULL) {
		return ENOMEM;
	}

	/* initialise fields */
	newas->as_stack = old->as_stack;
	newas->as_regions = NULL;

	/* create deep copy of regions */
	int error = copy_region(old->as_regions, newas->as_regions);

	if (error) {
		as_destroy(newas);
		return error;
	}

	/* copy over page table */
	error = vm_copyPTE(old->as_pagetable, newas->as_pagetable);

	if (error) {
		as_destroy(newas);
		return error;
	}

	*ret = newas;

	return 0;
}


void
as_destroy(struct addrspace *as)
{

	if (as == NULL) 
		return;
	
	/* clean up page table
	 * 1st level - 2^8 = 256 entries
	 * 2nd level - 2^6 = 64 entries 
	 * 3rd level - 2^6 = 64 entries
	 */
	
	// freeing pagetable
	if (as->as_pagetable != NULL)
		vm_freePT(as->as_pagetable);

	/* deallocate frames used */

	/* clean up list of region structs */
	region *curr = as->as_regions;

	while (curr != NULL) {
		region *to_free = curr;
		curr = curr->next;
		kfree(to_free);
	}

	kfree(as);
}

void
as_activate(void)
{

	/* copied from dumbvm */

	int i, spl;
	struct addrspace *as;

	as = proc_getas();

	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i = 0; i < NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{

	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
	as_activate();
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{

	// Error checking: Bad memory reference
	if (as == NULL) {
		panic("as==NULL");
		return EFAULT;
	}
		
	// Not enough spare memory on stack
	// changed this to > ...TODO 
	if (vaddr + memsize > as->as_stack) {
		return ENOMEM;
	}

	// page alignment from dumbvm.c
	/* ALIGN REGION */
	// Base
	memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;
	
	// Length
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

	region *new_regions = kmalloc(sizeof(region));
	
	// out of memory error
	if (new_regions == NULL) {
		kfree(new_regions);
		return ENOMEM;
	}

	new_regions->as_vaddr = vaddr;
	new_regions->size = memsize;
	new_regions->flags = 0;
	new_regions->next = NULL;

	// Set flags according to readable, writeable and executable
	if (readable) 
		new_regions->flags |= PF_R;

	if (writeable)
		new_regions->flags |= PF_W;
	
	if (executable) 
		new_regions->flags |= PF_X;
	
	new_regions->o_flags = new_regions->flags;

	/* add region to end of linked list */
	struct as_region *curr = as->as_regions;

	/* head of the list */
	if (curr == NULL) {
		as->as_regions = new_regions;
		return 0;
	}

	/* otherwise, loop through the linked list */
	while (curr != NULL && curr->next != NULL) {
		curr = curr->next;
	}

	curr->next = new_regions;

	return 0;
}

int
as_prepare_load(struct addrspace *as)
{

	//Error checking: bad memory reference
	if (as == NULL) {
		panic("as==NULL in prep");
		return EFAULT;
	}
	region *old_regions = as->as_regions;

	// loop through and set all readonly regions to readwrite for prepare load
	while (old_regions != NULL) {
		if ((old_regions->flags & PF_W) != PF_W) {
			old_regions->flags |= PF_W;
		}
		old_regions = old_regions->next;
	}

	return 0;
}

int
as_complete_load(struct addrspace *as)
{

	// Error checking: bad memory reference 
	if (as == NULL)  {
		panic("asnull in compl");
		return EFAULT;
	}

	region *old_regions = as->as_regions;
	while (old_regions != NULL) {
		// check if flags have been modified in prepare_load
		if (old_regions->flags == old_regions->o_flags)
			old_regions = old_regions->next;
		
		else {
			// set flags back to original flags
			old_regions->flags = old_regions->o_flags;
			old_regions = old_regions->next;
		}
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	int spl = splhigh();

	for (int i = 0; i < NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);

	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{

	// (void)as;
	as_define_region(as, USERSTACK - STACK_MEMSIZE, STACK_MEMSIZE, 1, 1, 0);
	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	as->as_stack = USERSTACK;

	return 0;
}

////////////////////////////////////////////////////////////
// 			HELPER FUNCTIONS
////////////////////////////////////////////////////////////

region *lookup_region(struct addrspace *as, vaddr_t faultaddress) {

    /* return region if found and NULL if not found */

    if (as == NULL) {
	    return NULL;
	}

    region *curr = as->as_regions;

    while (curr != NULL) {
		if (faultaddress >= curr->as_vaddr) {
			if ((faultaddress - curr->as_vaddr) < curr->size) {
				return curr;
			}
		}
        curr = curr->next;
	}

    return NULL;
}

paddr_t lookupPTE(struct addrspace *as, vaddr_t faultaddress) {

	paddr_t p_fault = KVADDR_TO_PADDR(faultaddress);

	if (as == NULL)
		return 0;

    paddr_t ***pagetable = as->as_pagetable;

    uint32_t msb = get_msb(p_fault);
    uint32_t ssb = get_ssb(p_fault);
    uint32_t lsb = get_lsb(p_fault);

	/* invalid translation */
    if (pagetable == NULL)
		return 0;

	else if (pagetable[msb] == NULL)
		return 0;
	
	else if (pagetable[msb][ssb] == NULL)
		return 0;

	/* page table entry exists in page table */
    paddr_t page_table_entry = pagetable[msb][ssb][lsb];

	return page_table_entry;
}

int copy_region(region *old_region, region *new_region) {

	region *current = old_region;  /* used to iterate over old_region list */
	region *new_tail = NULL;	   /* last node of the new list */
	
	new_region = NULL;

	/* no regions in old address space */
	if (current == NULL) {
		return 0;
	}

	while (current != NULL) {

		/* create a copy of the current node from old_region */
		region *new_node = create_copy_node(current);

		if (new_node == NULL) {
			return ENOMEM;
		}

		if (new_region == NULL) {
			/* set node to be the head of new_region list */ 
			new_region = new_node;
			new_tail = new_region;

		} else {
			/* add to the end of the list */
			new_tail->next = new_node;
			new_tail = new_tail->next;
		}
		
		/* iterate over old_region list */
		current = current->next;
	}

	return 0;
}

region *create_copy_node(region *node) {

	region *new_node = kmalloc(sizeof(region));

	if (new_node == NULL) {
		kfree(new_node);
		return NULL; /* ENOMEM */
	}

	/* copy fields of the struct over */
	new_node->as_vaddr = node->as_vaddr;
	new_node->size = node->size;
	new_node->flags = node->flags;
	new_node->o_flags = node->o_flags;
	new_node->next = NULL;

	return new_node;
}