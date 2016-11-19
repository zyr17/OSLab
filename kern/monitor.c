// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "BackTrace", mon_backtrace },
	{ "showmappings", "Show the page mappings between A and B", mon_showmappings },
	{ "changemappings", "Change the page mappings", mon_changemappings },
	{ "dumppmem", "Dump the content at physical address", mon_dumppmem },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

uint32_t mon_hexread(char *s){
	if (s[1] == 'x') s = s + 2;
	uint32_t re = 0;
	for (; s[0]; s ++ )
		if (s[0] >= '0' && s[0] <= '9')
			re = re * 16 + s[0] - '0';
		else if (s[0] >= 'a' && s[0] <= 'f')
			re = re * 16 + s[0] - 'a' + 10;
		else if (s[0] >= 'A' && s[0] <= 'F')
			re = re * 16 + s[0] - 'A' + 10;
		else{
			cprintf("Wrong hex num input, return 0.\n");
			return 0;
		}
	return re;
}

void bt_print(int now, uint32_t ebp, uint32_t eip){
    int i = 0;
    if (!ebp){
        return;
    }
	struct Eipdebuginfo inf;
	int ret = debuginfo_eip(eip, &inf);

	//if (ret < 0) cprintf("1111\n");
    cprintf("ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n    %s:%d: %.*s+%d\n", ebp, eip, *(uint32_t*)(ebp + 8), *(uint32_t*)(ebp + 12), *(uint32_t*)(ebp + 16), *(uint32_t*)(ebp + 20), *(uint32_t*)(ebp + 24), inf.eip_file, inf.eip_line, inf.eip_fn_namelen, inf.eip_fn_name, eip - inf.eip_fn_addr);
    ebp = *(uint32_t*)ebp;
    eip = *(uint32_t*)(ebp + 4);
    bt_print(now + 1, ebp, eip);
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	cprintf("Stack backtrace:\n");
	int ebp = read_ebp();
	bt_print(1, ebp, *(uint32_t*)(ebp + 4));
	return 0;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	if (argc < 3){
		cprintf("Input error! must be two hex num.\n");
		return 0;
	}
	uint32_t l, r;
	l = mon_hexread(argv[1]);
	r = mon_hexread(argv[2]);
	l &= 0xfffff000;
	r &= 0xfffff000;
	if (l > r){
		uint32_t t = l;
		l = r;
		r = t;
	}//cprintf("%x %x\n", l, r);
	uint32_t perm = 0xffffffff, last = 0;
	for (; ; l += PGSIZE){
		pte_t *pte = pgdir_walk(kern_pgdir, (void*)l, 0);
		uint32_t nowperm = 0xf0f0f0f0;
		if (pte != NULL && ((*pte) & PTE_P)) nowperm = (*pte) & 0x7;
		if (l < last || l > r) nowperm = 0xffffffff;
		if (nowperm != perm){//cprintf("%03x %03x ", perm, nowperm);
			if (perm == 0xffffffff){
			}
			else if (perm == 0xf0f0f0f0){
				cprintf("0x%08x - 0x%08x: NO PAGE\n", last, l - 1);
			}
			else if ((perm & PTE_U) && (perm & PTE_W)){
				cprintf("0x%08x - 0x%08x: User RW   | Kern RW\n", last, l - 1);
			}
			else if (perm & PTE_U){
				cprintf("0x%08x - 0x%08x: User R    | Kern R\n", last, l - 1);
			}
			else if (perm & PTE_W){
				cprintf("0x%08x - 0x%08x: User NONE | Kern RW\n", last, l - 1);
			}
			else{
				cprintf("0x%08x - 0x%08x: User NONE | Kern R\n", last, l - 1);
			}
			perm = nowperm;
			last = l;
		}
		if (nowperm == 0xffffffff) break;
	}
	return 0;
}

int
mon_changemappings(int argc, char **argv, struct Trapframe *tf)
{
	if (argc < 3){
		cprintf("Input error!\nchangemappings [vaddr] [-X]\n-p: with a hex num to change the permission.\n-a: with one hex num as physical memory address.\n    Note we can't select a physical page which is free or certain kernel page(i.e. pages with pp_ref = 0).\n    Or without anything to get a new page, the page will be cleared, permision will be USER: NONE | KERN: RW.\n-d: delete the page mapping.\n");
		return 0;
	}
	uint32_t l, r;
	l = mon_hexread(argv[1]);
	pte_t *pte = pgdir_walk(kern_pgdir, (void*)l, 0);
	if (argv[2][0] == '-' && argv[2][1] == 'p'){
		r = mon_hexread(argv[3]);
		l &= 0xfffff000;
		r &= 0xfff;
		if (pte != NULL){
			*pte = ((*pte) & 0xfffff000) + r;
			cprintf("Successfully changed!\n");
			char *arg[3];
			arg[0] = argv[0];
			arg[1] = argv[1];
			arg[2] = argv[1];
			mon_showmappings(argc, arg, tf);
		}
		else{
			cprintf("Page not exist!\n");
		}
	}
	else if (argv[2][0] == '-' && argv[2][1] == 'd'){
		if (pte == NULL){
			cprintf("Page not exist!\n");
		}
		else{
			page_remove(kern_pgdir, (void*)l);
			cprintf("Delete success!\n");
			char *arg[3];
			arg[0] = argv[0];
			arg[1] = argv[1];
			arg[2] = argv[1];
			mon_showmappings(argc, arg, tf);
		}
	}
	else if (argv[2][0] == '-' && argv[2][1] == 'a'){
		if (argc == 3){
			struct PageInfo *pp = page_alloc(ALLOC_ZERO);
			if (pp == NULL){
				cprintf("Out of memory!\n");
				page_free(pp);
			}
			else{
				uint32_t res = page_insert(kern_pgdir, pp, (void*)l, PTE_W | PTE_P);
				if (res){
					cprintf("Out of memory!\n");
					page_free(pp);
				}
				else{
					cprintf("Add success!\n");
					char *arg[3];
					arg[0] = argv[0];
					arg[1] = argv[1];
					arg[2] = argv[1];
					mon_showmappings(argc, arg, tf);
				}
			}
		}
		else{
			r = mon_hexread(argv[3]);
			struct PageInfo *pp = pages + (r >> 12);
			if ((r >> 12) >= npages){
				cprintf("Paddr too large!\n");
			}
			else{
				if (pp -> pp_ref == 0){
					cprintf("Error! pp -> ref = 0.\n");
				}
				else{
					uint32_t res = page_insert(kern_pgdir, pp, (void*)l, PTE_W | PTE_P);
					if (res){
						cprintf("Out of memory!\n");
						page_free(pp);
					}
					else{
						cprintf("Add success!\n");
						char *arg[3];
						arg[0] = argv[0];
						arg[1] = argv[1];
						arg[2] = argv[1];
						mon_showmappings(argc, arg, tf);
					}
				}
			}
		}
	}
	return 0;
}

int
mon_dumppmem(int argc, char **argv, struct Trapframe *tf)
{
	if (argc < 3){
		cprintf("Input error! Need one hex num: physical address and one hex num: N mean output N uint32_t num.");
		return 0;
	}
	uint32_t l, r;
	l = mon_hexread(argv[1]);
	r = mon_hexread(argv[2]);
	if (npages * 4096 <= l + r * 4){
		cprintf("Memory address out of bound.\n");
	}
	else{
		int tot = 0;
		for (; r; l += 0x4, r -- ){
			if (tot == 0){
				cprintf("0x%08x: ", l);
			}
			cprintf("0x%08x ", *(uint32_t*)(l + 0xf0000000));
			if ( ++ tot == 4){
				cprintf("\n");
				tot = 0;
			}
		}
	}
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
