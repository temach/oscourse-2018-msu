#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>
#include <inc/string.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>

#ifndef debug
# define debug 0
#endif

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};

/* interrupt handlers from trapentry.S */
void divide_thdlr();
void debug_thdlr();
void nmi_thdlr();
void brkpt_thdlr();
void oflow_thdlr();
void bound_thdlr();
void illop_thdlr();
void device_thdlr();
void dblflt_thdlr();

void tss_thdlr();
void segnp_thdlr();
void stack_thdlr();
void gpflt_thdlr();
void pgflt_thdlr();
void fperr_thdlr();
void align_thdlr();
void mchk_thdlr();
void simderr_thdlr();

void syscall_thdlr();
void kbd_thdlr();
void serial_thdlr();

static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}

void
trap_init(void)
{
	// extern struct Segdesc gdt[];
	int32_t i = 0;
	for (i = 0; i < 256; i++) {
		SETGATE(idt[i], 0, GD_KT, NULL, 0);
	}
	
	// LAB 8: Your code here.
	SETGATE(idt[T_DIVIDE], 0, GD_KT, (int)(& divide_thdlr), 0);
	SETGATE(idt[T_DEBUG ], 0, GD_KT, (int)(& debug_thdlr ), 0);
	SETGATE(idt[T_NMI   ], 0, GD_KT, (int)(& nmi_thdlr   ), 0);
	SETGATE(idt[T_BRKPT ], 0, GD_KT, (int)(& brkpt_thdlr ), 3);
	SETGATE(idt[T_OFLOW ], 0, GD_KT, (int)(& oflow_thdlr ), 0);
	SETGATE(idt[T_BOUND ], 0, GD_KT, (int)(& bound_thdlr ), 0);
	SETGATE(idt[T_ILLOP ], 0, GD_KT, (int)(& illop_thdlr ), 0);
	SETGATE(idt[T_DEVICE], 0, GD_KT, (int)(& device_thdlr), 0);
	SETGATE(idt[T_DBLFLT], 0, GD_KT, (int)(& dblflt_thdlr), 0);

	SETGATE(idt[T_TSS    ], 0, GD_KT, (int)(& tss_thdlr     ), 0);
	SETGATE(idt[T_SEGNP  ], 0, GD_KT, (int)(& segnp_thdlr   ), 0);
	SETGATE(idt[T_STACK  ], 0, GD_KT, (int)(& stack_thdlr   ), 0);
	SETGATE(idt[T_GPFLT  ], 0, GD_KT, (int)(& gpflt_thdlr   ), 0);
	SETGATE(idt[T_PGFLT  ], 0, GD_KT, (int)(& pgflt_thdlr   ), 0);
	SETGATE(idt[T_FPERR  ], 0, GD_KT, (int)(& fperr_thdlr   ), 0);
	SETGATE(idt[T_ALIGN  ], 0, GD_KT, (int)(& align_thdlr   ), 0);
	SETGATE(idt[T_MCHK   ], 0, GD_KT, (int)(& mchk_thdlr    ), 0);
	SETGATE(idt[T_SIMDERR], 0, GD_KT, (int)(& simderr_thdlr ), 0);

	SETGATE(idt[T_SYSCALL], 0, GD_KT, (int)(& syscall_thdlr ), 3);

	SETGATE(idt[IRQ_OFFSET + IRQ_KBD], 0, GD_KT, (int)(& kbd_thdlr ), 0);
	SETGATE(idt[IRQ_OFFSET + IRQ_SERIAL], 0, GD_KT, (int)(& serial_thdlr ), 0);

	// Per-CPU setup 
	trap_init_percpu();
}



// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;

	// Initialize the TSS slot of the gdt.
	gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate), 0);
	gdt[GD_TSS0 >> 3].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0);

	// Load the IDT
	lidt(&idt_pd);
}


void
clock_idt_init(void)
{
	extern void (*clock_thdlr)(void);
	// init idt structure
	SETGATE(idt[IRQ_OFFSET + IRQ_CLOCK], 0, GD_KT, (int)(&clock_thdlr), 0);
	lidt(&idt_pd);
}


void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	cprintf("  esp  0x%08x\n", tf->tf_esp);
	cprintf("  ss   0x----%04x\n", tf->tf_ss);
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}


static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.

	// Handle spurious interrupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	//
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}

	if (tf->tf_trapno == T_PGFLT) {
		page_fault_handler(tf);
		return;
	}
	
	if (tf->tf_trapno == T_SYSCALL) {
		struct PushRegs *r = &curenv->env_tf.tf_regs;
		int32_t tmp = syscall(r->reg_eax, r->reg_edx, r->reg_ecx, r->reg_ebx, r->reg_edi, r->reg_esi);
		r->reg_eax = tmp;
		return;
	}
	
	if (tf->tf_trapno == T_BRKPT) {
		monitor(tf);
		return;
	} 

	if (tf->tf_trapno == IRQ_OFFSET + IRQ_CLOCK) {
		// read RTC status reg just to recet the interrupt flag
		(void)rtc_check_status();
		// send EndOfInterrupt with IRQ_CLOCK as value
		pic_send_eoi(IRQ_CLOCK);
		sched_yield();
		return;
	}

	// Handle keyboard and serial interrupts.
	// LAB 11: Your code here.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_KBD) {
		kbd_intr();
		return;
	}

	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL) {
		serial_intr();
		return;
	}

	print_trapframe(tf);
	if (tf->tf_cs == GD_KT) {
		panic("unhandled trap in kernel");
	} else {
		env_destroy(curenv);
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Halt the CPU if some other CPU has called panic()
	extern char *panicstr;
	if (panicstr)
		asm volatile("hlt");

	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	if (debug) {
		cprintf("Incoming TRAP frame at %p\n", tf);
	}

	assert(curenv);

	// Garbage collect if current enviroment is a zombie
	if (curenv->env_status == ENV_DYING) {
		env_free(curenv);
		curenv = NULL;
		sched_yield();
	}

	// Copy trap frame (which is currently on the stack)
	// into 'curenv->env_tf', so that running the environment
	// will restart at the trap point.
	curenv->env_tf = *tf;
	// The trapframe on the stack should be ignored from here on.
	tf = &curenv->env_tf;

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.
	if ((tf->tf_cs & 3) == 0) {
		// just panic, nothing we can do
		panic("kernel mode page fault");
	}

	// LAB 8: Your code here.

	// page 95 of Programming on asm on platform x86-64 by Ruslan Ablyazov
	// uint32_t error_code = tf->tf_err;
	// if (error_code & PTE_P) {
	// 	// permission error
	// 	// just terminate
	// } else {
	//	// error due to absent page
	// }
	//
	// if (error_code & PTE_U) {
	// 	// error made by user space code
	// } else {
	//	// error made by kernel code
	// }
	//
	// if (error_code & PTE_W) {
	// 	// error made while writing to address
	// } else {
	//	// error made by reading from address
	// }

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return.  In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free.  In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	// LAB 9: Your code here.
	if (! curenv->env_pgfault_upcall) {
		// if no upcall found, just kill
		// Destroy the environment that caused the fault.
		cprintf("[%08x] user fault va %08x ip %08x\n",
				curenv->env_id, fault_va, tf->tf_eip);
		print_trapframe(tf);
		env_destroy(curenv);
	}

	// first cehck if we are in user stack or exception stack
	char* user_trap_frame_addr = 0;
	if (UXSTACKTOP - PGSIZE <= tf->tf_esp && tf->tf_esp <= UXSTACKTOP - 1) {
		// we are in exception stack, the fault handler gave an exception
		// must add empty 32-bits, before insterting the next trapframe
		user_trap_frame_addr = (char*)tf->tf_esp - 4;
	} else {
		user_trap_frame_addr = (char*)UXSTACKTOP;
	}


	struct UTrapframe *user_tf = (struct UTrapframe*)(user_trap_frame_addr - sizeof (struct UTrapframe));
	user_mem_assert(curenv, user_tf, sizeof(struct UTrapframe), PTE_U | PTE_W);
	user_tf->utf_eflags = tf->tf_eflags;
	user_tf->utf_eip = tf->tf_eip;
	user_tf->utf_esp = tf->tf_esp;
	user_tf->utf_err = tf->tf_err;
	user_tf->utf_regs = tf->tf_regs;
	user_tf->utf_fault_va = fault_va;

	// now allow to execute the user handler
	tf->tf_esp = (uint32_t)user_tf;
	tf->tf_eip = (uint32_t)curenv->env_pgfault_upcall;
	env_run(curenv);
}

