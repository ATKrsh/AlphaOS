// isr.h
// Declarations for Interrupt Service Routines (ISRs) and Interrupt Requests (IRQs)

#ifndef ISR_H
#define ISR_H

// Exception interrupt names
#define INT_DIV_BY_ZERO         0
#define INT_DEBUG               1
#define INT_NMI                 2
#define INT_BREAKPOINT          3
#define INT_OVERFLOW            4
#define INT_BOUNDS              5
#define INT_INVALID_OPCODE      6
#define INT_NO_COPROCESSOR      7
#define INT_DOUBLE_FAULT        8
#define INT_COPROC_SEG_OVERRUN  9
#define INT_BAD_TSS             10
#define INT_SEG_NOT_PRESENT     11
#define INT_STACK_FAULT         12
#define INT_GPF                 13
#define INT_PAGE_FAULT          14
#define INT_UNKNOWN             15
#define INT_COPROC_FAULT        16
#define INT_ALIGNMENT_CHECK     17
#define INT_MACHINE_CHECK       18
#define INT_SIMD_FAULT          19
#define INT_VIRTUALIZATION      20
#define INT_SECURITY            30

// Mapped IRQ interrupt numbers
#define IRQ0  32 // Timer
#define IRQ1  33 // Keyboard
#define IRQ2  34 // Cascade
#define IRQ3  35 // COM2
#define IRQ4  36 // COM1
#define IRQ5  37 // LPT2
#define IRQ6  38 // Floppy
#define IRQ7  39 // LPT1
#define IRQ8  40 // CMOS Clock
#define IRQ9  41 // ACPI
#define IRQ10 42 // NIC
#define IRQ11 43 // NIC
#define IRQ12 44 // Mouse
#define IRQ13 45 // Coprocessor
#define IRQ14 46 // Primary ATA
#define IRQ15 47 // Secondary ATA

// CPU register structure passed from interrupt.asm
struct registers {
    unsigned int ds;                                      // Data segment selector
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
    unsigned int int_no, err_code;                        // Pushed manually in ASM stubs
    unsigned int eip, cs, eflags, useresp, ss;            // Pushed automatically by CPU
};

// Interrupt handler function pointer type
typedef void (*isr_t)(struct registers*);

// Registers an IRQ or ISR handler
void register_interrupt_handler(unsigned char n, isr_t handler);

// PIC management
void pic_remap();
void pic_send_eoi(unsigned char irq);

// Timer PIT initialization
void pit_init(unsigned int frequency);

// C Dispatchers called by interrupt.asm
void isr_handler(struct registers r);
void irq_handler(struct registers r);

#endif
