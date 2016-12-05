// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
	//cprintf("in pgfault:addr:%x, err: %x\n", addr, err);
	//if ((uint32_t)addr >= 0x802000 && (uint32_t)addr < 0x803000) cprintf("\n\n\n\n\n\n%x\n\n\n\n\n",*(uint32_t*)(0x802008));

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if (!(err & FEC_WR)) panic("in pgfault: %e\n", err);
	if (!(uvpt[(uint32_t)addr >> 12] & PTE_COW)) panic("in pgfault: not copy-on-write\n");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	int envid = sys_getenvid(), res;
	res = sys_page_alloc(envid, PFTEMP, PTE_P | PTE_U | PTE_W);
	if (res < 0) panic("in pgfault: %e\n", res);
	memcpy(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
	res = sys_page_map(envid, PFTEMP, envid, ROUNDDOWN(addr, PGSIZE), PTE_P | PTE_U | PTE_W);
	if (res < 0) panic("in pgfault: %e\n", res);
	res = sys_page_unmap(envid, PFTEMP);
	if (res < 0) panic("in pgfault: %e\n", res);
	//cprintf("pgfault ret\n");
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	//panic("duppage not implemented");
	int perm = uvpt[pn] & 0xfff;
	if ((perm & PTE_W) || (perm & PTE_COW))
		perm = (perm & ~PTE_W) | PTE_COW;
	perm = perm & ~(PTE_A | PTE_D);
	int fenvid = sys_getenvid(), res;
	//cprintf("%x %x %x %x\n", envid, pn, uvpt[pn] & 0xfff, perm);
	res = sys_page_map(fenvid, (void*)(pn * PGSIZE), envid, (void*)(pn * PGSIZE), perm);
	if (res < 0) panic("in duppage: %e\n", res);
	res = sys_page_map(fenvid, (void*)(pn * PGSIZE), fenvid, (void*)(pn * PGSIZE), perm);
	if (res < 0) panic("in duppage: %e\n", res);
	//cprintf("ret\n");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	//panic("fork not implemented");
	set_pgfault_handler(pgfault);
	int cenvid = sys_exofork();
	if (cenvid == 0){
		//cprintf("child %x\n", sys_getenvid());
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	//cprintf("father %x\n", sys_getenvid());
	if (cenvid < 0) cprintf("in fork: %e\n", cenvid);
	int envid = sys_getenvid();
	int i;
	for (i = 0; i < UTOP >> 12; i ++ ){
		//if ((uvpd[i >> 10] & PTE_P) && (uvpt[i] & PTE_P)) cprintf("do %x\n", i);
		if ((uvpd[i >> 10] & PTE_P) && (uvpt[i] & PTE_P) && i * PGSIZE != UXSTACKTOP - PGSIZE) duppage(cenvid, i);
	}
	int res;
	//cprintf("alloc uxstack\n");
	res = sys_page_alloc(envid, PFTEMP, PTE_P | PTE_U | PTE_W);
	if (res < 0) panic("in fork: %e\n", res);
	//cprintf("alloc ok\n");
	memcpy(PFTEMP, (void*)(UXSTACKTOP - PGSIZE), PGSIZE);
	res = sys_page_map(envid, PFTEMP, cenvid, (void*)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W);
	if (res < 0) panic("in fork: %e\n", res);
	res = sys_page_unmap(envid, PFTEMP);
	if (res < 0) panic("in fork: %e\n", res);
	void _pgfault_upcall();
	res = sys_env_set_pgfault_upcall(cenvid, _pgfault_upcall);
	if (res < 0) panic("in fork: %e\n", res);
	sys_env_set_status(cenvid, ENV_RUNNABLE);
	return cenvid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
