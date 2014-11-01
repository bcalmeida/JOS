/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


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
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

# Handlers for process exceptions
TRAPHANDLER_NOEC(handler0, 0)
TRAPHANDLER_NOEC(handler1, 1)
TRAPHANDLER_NOEC(handler2, 2)
TRAPHANDLER_NOEC(handler3, 3)
TRAPHANDLER_NOEC(handler4, 4)
TRAPHANDLER_NOEC(handler5, 5)
TRAPHANDLER_NOEC(handler6, 6)
TRAPHANDLER_NOEC(handler7, 7)
TRAPHANDLER(handler8, 8)
TRAPHANDLER_NOEC(handler9, 9)
TRAPHANDLER(handler10, 10)
TRAPHANDLER(handler11, 11)
TRAPHANDLER(handler12, 12)
TRAPHANDLER(handler13, 13)
TRAPHANDLER(handler14, 14)
TRAPHANDLER_NOEC(handler15, 15)
TRAPHANDLER_NOEC(handler16, 16)
TRAPHANDLER(handler17, 17)
TRAPHANDLER_NOEC(handler18, 18)
TRAPHANDLER_NOEC(handler19, 19)

# Handlers for external interrupts
TRAPHANDLER_NOEC(handler_irq0, IRQ_OFFSET + 0)
TRAPHANDLER_NOEC(handler_irq1, IRQ_OFFSET + 1)
TRAPHANDLER_NOEC(handler_irq2, IRQ_OFFSET + 2)
TRAPHANDLER_NOEC(handler_irq3, IRQ_OFFSET + 3)
TRAPHANDLER_NOEC(handler_irq4, IRQ_OFFSET + 4)
TRAPHANDLER_NOEC(handler_irq5, IRQ_OFFSET + 5)
TRAPHANDLER_NOEC(handler_irq6, IRQ_OFFSET + 6)
TRAPHANDLER_NOEC(handler_irq7, IRQ_OFFSET + 7)
TRAPHANDLER_NOEC(handler_irq8, IRQ_OFFSET + 8)
TRAPHANDLER_NOEC(handler_irq9, IRQ_OFFSET + 9)
TRAPHANDLER_NOEC(handler_irq10,IRQ_OFFSET + 10)
TRAPHANDLER_NOEC(handler_irq11,IRQ_OFFSET + 11)
TRAPHANDLER_NOEC(handler_irq12,IRQ_OFFSET + 12)
TRAPHANDLER_NOEC(handler_irq13,IRQ_OFFSET + 13)
TRAPHANDLER_NOEC(handler_irq14,IRQ_OFFSET + 14)
TRAPHANDLER_NOEC(handler_irq15,IRQ_OFFSET + 15)

# For system call
TRAPHANDLER_NOEC(handler_syscall, T_SYSCALL)

/*
 * Lab 3: Your code here for _alltraps
 */
// TODO: Replace mov with movw
.globl _alltraps
_alltraps:
	# Push values to make the stack look like a struct Trapframe
	pushl %ds
	pushl %es
	pushal

	# Load GD_KD into %ds and %es
	mov $GD_KD, %eax
	mov %ax, %ds
	mov %ax, %es

	# Call trap(tf), where tf=%esp
	pushl %esp
	call trap
	addl $4, %esp

# Used for Lab 3 - Challenge. It is used to make a loop in C code.
.data
.globl handlers
handlers:
	.long handler0
	.long handler1
	.long handler2
	.long handler3
	.long handler4
	.long handler5
	.long handler6
	.long handler7
	.long handler8
	.long handler9
	.long handler10
	.long handler11
	.long handler12
	.long handler13
	.long handler14
	.long handler15
	.long handler16
	.long handler17
	.long handler18
	.long handler19

.globl handlers_irq
handlers_irq:
	.long handler_irq0
	.long handler_irq1
	.long handler_irq2
	.long handler_irq3
	.long handler_irq4
	.long handler_irq5
	.long handler_irq6
	.long handler_irq7
	.long handler_irq8
	.long handler_irq9
	.long handler_irq10
	.long handler_irq11
	.long handler_irq12
	.long handler_irq13
	.long handler_irq14
	.long handler_irq15
