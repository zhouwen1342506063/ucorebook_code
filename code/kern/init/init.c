#include <types.h>
#include <x86.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <kdebug.h>
#include <picirq.h>
#include <trap.h>
#include <clock.h>
#include <intr.h>
#include <memlayout.h>
#include <pmm.h>

int kern_init(void) __attribute__((noreturn));

static void switch_test(void);

int
kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    cons_init();                // init the console

    const char *message = "(THU.CST) os is loading ...";
    cprintf("%s\n\n", message);

    print_kerninfo();

    pmm_init();                 // init physical memory management

    pic_init();                 // init interrupt controller
    idt_init();                 // init interrupt descriptor table

    clock_init();               // init clock interrupt
    intr_enable();              // enable irq interrupt

    // user/kernel mode switch test
    switch_test();

    /* do nothing */
    while (1);
}

static void
print_cur_status(void) {
    static int round = 0;
    uint16_t reg1, reg2, reg3, reg4;
    asm volatile (
            "mov %%cs, %0;"
            "mov %%ds, %1;"
            "mov %%es, %2;"
            "mov %%ss, %3;"
            : "=m" (reg1), "=m" (reg2), "=m" (reg3), "=m" (reg4));
    cprintf("%d: @ring %d\n", round, reg1 & 3);
    cprintf("%d:  cs = %x\n", round, reg1);
    cprintf("%d:  ds = %x\n", round, reg2);
    cprintf("%d:  es = %x\n", round, reg3);
    cprintf("%d:  ss = %x\n", round, reg4);
    round ++;
}

static void switch_to_user(void) __attribute__((noinline));
static void switch_to_kernel(void) __attribute__((noinline));

static void
switch_to_user(void) {
    uint32_t eflags = read_eflags();
    write_eflags(eflags | (3 << 12));

    asm volatile (
            "pushl %%ebp;"          // save %ebp
            "movl %%esp, %%ebp;"    // move %esp -> %ebp
            "pushl %1;"             // push %ss
            "pushl %%ebp;"          // push %esp
            "pushl %0;"             // push %cs
            "pushl $label1;"        // push %eip
            "lret;"                 // ring0 -> ring3
            "label1:"
            "popl %%ebp;"           // restore %ebp
            "push %%ax;"            // set %ds, %es
            "mov %1, %%ax;"
            "mov %%ax, %%ds;"
            "mov %%ax, %%es;"
            "pop %%ax;"
            :: "i" (USER_CS), "i" (USER_DS));
}

static void
switch_to_kernel(void) {
    asm volatile (
            "pushl %%ebp;"
            "lcall %0, $0x0;"
            "popl %%ebp;"
            :: "i" ((SEG_CG << 3) | DPL_KERNEL));
}

static void
switch_test(void) {
    print_cur_status();
    cprintf("+++ switch to  user  mode +++\n");
    switch_to_user();
    print_cur_status();
    cprintf("+++ switch to kernel mode +++\n");
    switch_to_kernel();
    print_cur_status();
}

