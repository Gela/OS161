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
#include <addrspace.h>
#include <spl.h>
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <../include/vm.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	as->pages = NULL;
	as->regions = NULL;
	as->stack = NULL;
	as->heap = NULL;
	as->heap_end = (vaddr_t)0;
	as->heap_start = (vaddr_t)0;

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */
//	new->heap_start=old->heap_start;
//	new->heap_end=old->heap_end;
//
//	KASSERT(new->heap_start != 0);
//	KASSERT(new->heap_end != 0);


	// Setup All the regions
	struct regionlist * itr,*newitr,*tmp;

	itr=old->regions;

	while(itr!=NULL){
		if(new->regions == NULL){
			new->regions=(struct regionlist *) kmalloc(sizeof(struct regionlist));
			new->regions->next = NULL;
			newitr=new->regions;
		}
		else{
			for(tmp=new->regions;tmp->next!=NULL;tmp=tmp->next);
			newitr = (struct regionlist *) kmalloc(sizeof(struct regionlist));
			tmp->next=newitr;
		}

		newitr->vbase=itr->vbase;
		newitr->pbase=itr->pbase;
		newitr->npages=itr->npages;
		newitr->permissions=itr->permissions;
		newitr->next=NULL;

		itr = itr->next;
		//newitr->last=itr->last;

	}

	// Now actually allocate new pages for these regions
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	// Copy the data from old to new
	struct page_table_entry *iterate1 = old->pages;
	struct page_table_entry *iterate2 = new->pages;

	while(iterate1!=NULL){

		memmove((void *)PADDR_TO_KVADDR(iterate2->paddr),
				(const void *)PADDR_TO_KVADDR(iterate1->paddr),PAGE_SIZE);

		iterate1 = iterate1->next;
		iterate2 = iterate2->next;
	}

	*ret = new;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
//	kprintf("Currently free pages in coremap (as_destroy before)- %d\n",corefree());
	if(as!=NULL){

		struct regionlist * reglst=as->regions;
		struct regionlist * temp;
		while(reglst){
			temp=reglst;
			reglst=reglst->next;
			kfree(temp);
		}

		struct page_table_entry * pte=as->pages;
		struct page_table_entry * pagetemp;
		while(pte != NULL){

			pagetemp=pte;
			pte=pte->next;

			free_upages(pagetemp->paddr);

			kfree(pagetemp);

		}
	}
//	kprintf("Currently free pages in coremap (as_destroy after)- %d\n",corefree());
	kfree(as);
}

//static
//void
//as_zero_region(paddr_t paddr, unsigned npages)
//{
//	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
//}

void
as_activate(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	int i, spl;

	(void)as;

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
	//(void)as;  // suppress warning until code gets written
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
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */
	size_t npages;

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */

	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	//	kprintf("Region VADDR %d\n",vaddr);
	//	kprintf("Region Pages %d\n",npages);

	// new code
	struct regionlist * end;
	if(as->regions != NULL){
		end = as->regions->last;
	}

	if (as->regions == NULL) {
		as->regions =(struct regionlist *) kmalloc(sizeof(struct regionlist));
		as->regions->next = NULL;
		as->regions->last= as->regions;
		end=as->regions;
	}
	else {
		end=as->regions->last;
		end->next = (struct regionlist *) kmalloc(sizeof(struct regionlist));
		end=end->next;
		end->next = NULL;
		as->regions->last=end;
	}

	end->vbase = vaddr;
	end->npages = npages;
	end->pbase = 0;
	end->permissions = 7 & (readable | writeable | executable); // CHECK THIS LATER!!

	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	paddr_t paddr;
	vaddr_t vaddr;

	//as->stackpbase = paddr;
	//as_zero_region(as->stackpbase, DUMBVM_STACKPAGES);

	// Setting up page table
	struct regionlist * regionlst;
	struct page_table_entry * pages;
	regionlst=as->regions;
	size_t i;
	while(regionlst != NULL){
		vaddr=regionlst->vbase;
		for(i=0;i<regionlst->npages;i++){
			if(as->pages==NULL){

				as->pages = (struct page_table_entry *)kmalloc(sizeof(struct page_table_entry));
				as->pages->vaddr = vaddr;
				as->pages->permissions = regionlst->permissions;
				as->pages->next = NULL;
				paddr = alloc_upages(1);
				if(paddr == 0){
					return ENOMEM;
				}

				as->pages->paddr = paddr;


			}else{
				for(pages=as->pages;pages->next!=NULL;pages=pages->next);
				pages->next = (struct page_table_entry *)kmalloc(sizeof(struct page_table_entry));
				pages->next->vaddr = vaddr;
				pages->next->permissions = regionlst->permissions; // CHECK THIS LATER!!
				pages->next->next = NULL;
				paddr = alloc_upages(1);
				if(paddr == 0){
					return ENOMEM;
				}
				pages->next->paddr = paddr;


			}

			vaddr += PAGE_SIZE;
		}


		regionlst=regionlst->next;

	}

	// New Code
	vaddr_t stackvaddr = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	for(pages=as->pages;pages->next!=NULL;pages=pages->next);
	for(int i=0; i<DUMBVM_STACKPAGES; i++){
		struct page_table_entry *stack = (struct page_table_entry *)kmalloc(sizeof(struct page_table_entry));
		pages->next = stack;
		if(i==0){
			as->stack = stack;
		}
		stack->vaddr = stackvaddr;
		stack->next = NULL;
		paddr = alloc_upages(1);
		if(paddr == 0){
			return ENOMEM;
		}
		stack->paddr = paddr;



		stackvaddr = stackvaddr + PAGE_SIZE;
		pages = pages->next;
	}

	//

	struct page_table_entry *heap_page = (struct page_table_entry *)kmalloc(sizeof(struct page_table_entry));
	pages->next = heap_page;
	heap_page->next = NULL;

	paddr = alloc_upages(1);
	if(paddr == 0){
		return ENOMEM;
	}

	heap_page->paddr = paddr;
	heap_page->vaddr = vaddr;

	as->heap_start = as->heap_end = vaddr;
	as->heap = heap_page;

	KASSERT(as->heap_start != 0);
	KASSERT(as->heap_end != 0);

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

