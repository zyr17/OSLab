/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
.text;                                    \
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps;             \
.data;            \
    .long name

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
.text;                                    \
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps;             \
.data;                    \
    .long name
    
 #define TRAPHANDLER_RESERVED					\
.data;                    \
    .long 0

.data
.globl handler_funcs
handler_funcs:

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

TRAPHANDLER_NOEC(_handler_0, T_DIVIDE);
TRAPHANDLER_NOEC(_handler_1, T_DEBUG);
TRAPHANDLER_NOEC(_handler_2, T_NMI);
TRAPHANDLER_NOEC(_handler_3, T_BRKPT);
TRAPHANDLER_NOEC(_handler_4, T_OFLOW);
TRAPHANDLER_NOEC(_handler_5, T_BOUND);
TRAPHANDLER_NOEC(_handler_6, T_ILLOP);
TRAPHANDLER_NOEC(_handler_7, T_DEVICE);
TRAPHANDLER(_handler_8, T_DBLFLT);
TRAPHANDLER_RESERVED;
TRAPHANDLER(_handler_10, T_TSS);
TRAPHANDLER(_handler_11, T_SEGNP);
TRAPHANDLER(_handler_12, T_STACK);
TRAPHANDLER(_handler_13, T_GPFLT);
TRAPHANDLER(_handler_14, T_PGFLT);
TRAPHANDLER_RESERVED;//0xf
TRAPHANDLER_NOEC(_handler_16, T_FPERR);
.data; .fill 15, 4, 0;//0x1f
.data; .fill 16, 4, 0;//0x2f
TRAPHANDLER_NOEC(_handler_48, T_SYSCALL);
.data; .fill 15, 4, 0;//0x3f
.data; .fill 192, 4, 0;//0xff

/*
 * Lab 3: Your code here for _alltraps
 */
 
_alltraps:
pushw $0;
pushw %ds;
pushw $0;
pushw %es;
pushal;
movw $GD_KD, %ax;
movw %ax, %ds;
movw %ax, %es;
pushl %esp;
call trap;

