/*
 * Copyright (c) 2014, Steffen Vogel, RWTH Aachen University
 *                     All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the University nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hermit/vma.h>
#include <hermit/stdlib.h>
#include <hermit/stdio.h>
#include <hermit/tasks_types.h>
#include <hermit/spinlock.h>
#include <hermit/errno.h>

/* 
 * Note that linker symbols are not variables, they have no memory allocated for
 * maintaining a value, rather their address is their value.
 */
extern const void kernel_start;
extern const void kernel_end;

#define PAGE_2M_FLOOR(addr)	(((addr) + (1L << 21) - 1) & ((~0L) << 21))
#define PAGE_2M_CEIL(addr)	( (addr)                   & ((~0L) << 21))

/*
 * Kernel space VMA list and lock
 * 
 * For bootstrapping we initialize the VMA list with one empty VMA
 * (start == end) and expand this VMA by calls to vma_alloc()
 */
static vma_t vma_boot = { VMA_MIN, VMA_MIN, VMA_HEAP };
static vma_t* vma_list = &vma_boot;
static spinlock_t vma_lock = SPINLOCK_INIT;

// TODO: we might move the architecture specific VMA regions to a
//       seperate function arch_vma_init()
int vma_init(void)
{
	int ret;

	kprintf("vma_init: reserve vma region 0x%llx - 0x%llx\n",
		PAGE_2M_CEIL((size_t) &kernel_start),
		PAGE_2M_FLOOR((size_t) &kernel_end));

	// add Kernel
	ret  = vma_add(PAGE_2M_CEIL((size_t) &kernel_start),
		PAGE_2M_FLOOR((size_t) &kernel_end),
		VMA_READ|VMA_WRITE|VMA_EXECUTE|VMA_CACHEABLE);
	if (BUILTIN_EXPECT(ret, 0))
		goto out;

out:
	return ret;
}

size_t vma_alloc(size_t size, uint32_t flags)
{
	spinlock_t* lock = &vma_lock;
	vma_t** list = &vma_list;

	//kprintf("vma_alloc: size = %#lx, flags = %#x\n", size, flags);

	// boundaries of free gaps
	size_t start, end;

	// boundaries for search
	size_t base = VMA_MIN;
	size_t limit = VMA_MAX;

	spinlock_lock(lock);

	// first fit search for free memory area
	vma_t* pred = NULL;  // vma before current gap
	vma_t* succ = *list; // vma after current gap
	do {
		start = (pred) ? pred->end : base;
		end = (succ) ? succ->start : limit;

		if (start + size < end && start >= base && start + size < limit)
			goto found; // we found a gap which is large enough and in the bounds

		pred = succ;
		succ = (pred) ? pred->next : NULL;
	} while (pred || succ);

fail:
	spinlock_unlock(lock);	// we were unlucky to find a free gap

	return 0;

found:
	if (pred && pred->flags == flags) {
		pred->end += size; // resize VMA
		//kprintf("vma_alloc: resize vma, start 0x%zx, pred->start 0x%zx, pred->end 0x%zx\n", start, pred->start, pred->end);
	} else {
		// insert new VMA
		vma_t* new = kmalloc(sizeof(vma_t));
		if (BUILTIN_EXPECT(!new, 0))
			goto fail;

		new->start = start;
		new->end = start + size;
		new->flags = flags;
		new->next = succ;
		new->prev = pred;

		if (succ)
			succ->prev = new;
		if (pred)
			pred->next = new;
		else
			*list = new;
	}

	spinlock_unlock(lock);

	return start;
}

int vma_free(size_t start, size_t end)
{
	spinlock_t* lock = &vma_lock;
	vma_t* vma;
	vma_t** list = &vma_list;

	//kprintf("vma_free: start = %#lx, end = %#lx\n", start, end);

	if (BUILTIN_EXPECT(start >= end, 0))
		return -EINVAL;

	spinlock_lock(lock);

	// search vma
	vma = *list;
	while (vma) {
		if (start >= vma->start && end <= vma->end) break;
		vma = vma->next;
	}

	if (BUILTIN_EXPECT(!vma, 0)) {
		spinlock_unlock(lock);
		return -EINVAL;
	}

	// free/resize vma
	if (start == vma->start && end == vma->end) {
		if (vma == *list)
			*list = vma->next; // update list head
		if (vma->prev)
			vma->prev->next = vma->next;
		if (vma->next)
			vma->next->prev = vma->prev;
		kfree(vma);
	}
	else if (start == vma->start)
		vma->start = end;
	else if (end == vma->end)
		vma->end = start;
	else {
		vma_t* new = kmalloc(sizeof(vma_t));
		if (BUILTIN_EXPECT(!new, 0)) {
			spinlock_unlock(lock);
			return -ENOMEM;
		}

		new->flags = vma->flags;

		new->end = vma->end;
		vma->end = start;
		new->start = end;

		new->next = vma->next;
		vma->next = new;
		new->prev = vma;
	}

	spinlock_unlock(lock);

	return 0;
}

int vma_add(size_t start, size_t end, uint32_t flags)
{
	spinlock_t* lock = &vma_lock;
	vma_t** list = &vma_list;

	if (BUILTIN_EXPECT(start >= end, 0))
		return -EINVAL;

	//kprintf("vma_add: start = %#lx, end = %#lx, flags = %#x\n", start, end, flags);

	spinlock_lock(lock);

	// search gap
	vma_t* pred = NULL;
	vma_t* succ = *list;

	while (pred || succ) {
		if ((!pred || pred->end <= start) &&
		    (!succ || succ->start >= end))
			break;

		pred = succ;
		succ = (succ) ? succ->next : NULL;
	}

	if (BUILTIN_EXPECT(*list && !pred && !succ, 0)) {
		spinlock_unlock(lock);
		return -EINVAL;
	}

	// insert new VMA
	vma_t* new = kmalloc(sizeof(vma_t));
	if (BUILTIN_EXPECT(!new, 0)) {
		spinlock_unlock(lock);
		return -ENOMEM;
	}

	new->start = start;
	new->end = end;
	new->flags = flags;
	new->next = succ;
	new->prev = pred;

	if (succ)
		succ->prev = new;
	if (pred)
		pred->next = new;
	else
		*list = new;

	spinlock_unlock(lock);

	return 0;
}

void vma_dump(void)
{
	void print_vma(vma_t *vma) {
		while (vma) {
			kprintf("0x%lx - 0x%lx: size=%x, flags=%c%c%c%s\n", vma->start, vma->end, vma->end - vma->start,
				(vma->flags & VMA_READ) ? 'r' : '-',
				(vma->flags & VMA_WRITE) ? 'w' : '-',
				(vma->flags & VMA_EXECUTE) ? 'x' : '-',
				(vma->flags & VMA_CACHEABLE) ? "" : " (uncached)");
			vma = vma->next;
		}
	}

	kputs("VMAs:\n");
	spinlock_lock(&vma_lock);
	print_vma(&vma_boot);
	spinlock_unlock(&vma_lock);
}
