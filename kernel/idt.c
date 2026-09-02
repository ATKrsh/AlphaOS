// idt.c
// Implementation for setting up the Interrupt Descriptor Table (IDT)

#include "idt.h"

// IDT array of 256 gates
struct idt_entry idt[256];
struct idt_ptr idt_p;

// Declare the external assembly interrupt handlers (defined in interrupt.asm)
extern void isr0();  extern void isr1();  extern void isr2();  extern void isr3();
extern void isr4();  extern void isr5();  extern void isr6();  extern void isr7();
extern void isr8();  extern void isr9();  extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();

extern void irq0();  extern void irq1();  extern void irq2();  extern void irq3();
extern void irq4();  extern void irq5();  extern void irq6();  extern void irq7();
extern void irq8();  extern void irq9();  extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();

// Configures a specific IDT gate
void idt_set_gate(int n, unsigned int handler) {
    if (handler == 0) return;
    
    idt[n].low_offset = handler & 0xFFFF;
    idt[n].sel = KERNEL_CS;
    idt[n].always0 = 0;
    // Flags: Present=1, Privilege=00 (Ring 0), 32-bit Interrupt Gate (0xE) -> 10001110b = 0x8E
    idt[n].flags = 0x8E;
    idt[n].high_offset = (handler >> 16) & 0xFFFF;
}

// Installs and loads the IDT
void idt_install() {
    // Set up the IDT pointer
    idt_p.limit = (sizeof(struct idt_entry) * 256) - 1;
    idt_p.base  = (unsigned int)&idt;

    // Zero out the entire IDT initially
    for (int i = 0; i < 256; i++) {
        idt[i].low_offset = 0;
        idt[i].sel = 0;
        idt[i].always0 = 0;
        idt[i].flags = 0;
        idt[i].high_offset = 0;
    }

    // Map Exceptions (0 - 31)
    idt_set_gate(0,  (unsigned int)isr0);
    idt_set_gate(1,  (unsigned int)isr1);
    idt_set_gate(2,  (unsigned int)isr2);
    idt_set_gate(3,  (unsigned int)isr3);
    idt_set_gate(4,  (unsigned int)isr4);
    idt_set_gate(5,  (unsigned int)isr5);
    idt_set_gate(6,  (unsigned int)isr6);
    idt_set_gate(7,  (unsigned int)isr7);
    idt_set_gate(8,  (unsigned int)isr8);
    idt_set_gate(9,  (unsigned int)isr9);
    idt_set_gate(10, (unsigned int)isr10);
    idt_set_gate(11, (unsigned int)isr11);
    idt_set_gate(12, (unsigned int)isr12);
    idt_set_gate(13, (unsigned int)isr13);
    idt_set_gate(14, (unsigned int)isr14);
    idt_set_gate(15, (unsigned int)isr15);
    idt_set_gate(16, (unsigned int)isr16);
    idt_set_gate(17, (unsigned int)isr17);
    idt_set_gate(18, (unsigned int)isr18);
    idt_set_gate(19, (unsigned int)isr19);
    idt_set_gate(20, (unsigned int)isr20);
    idt_set_gate(21, (unsigned int)isr21);
    idt_set_gate(22, (unsigned int)isr22);
    idt_set_gate(23, (unsigned int)isr23);
    idt_set_gate(24, (unsigned int)isr24);
    idt_set_gate(25, (unsigned int)isr25);
    idt_set_gate(26, (unsigned int)isr26);
    idt_set_gate(27, (unsigned int)isr27);
    idt_set_gate(28, (unsigned int)isr28);
    idt_set_gate(29, (unsigned int)isr29);
    idt_set_gate(30, (unsigned int)isr30);
    idt_set_gate(31, (unsigned int)isr31);

    // Map Hardware Interrupts / IRQs (32 - 47)
    idt_set_gate(32, (unsigned int)irq0);
    idt_set_gate(33, (unsigned int)irq1);
    idt_set_gate(34, (unsigned int)irq2);
    idt_set_gate(35, (unsigned int)irq3);
    idt_set_gate(36, (unsigned int)irq4);
    idt_set_gate(37, (unsigned int)irq5);
    idt_set_gate(38, (unsigned int)irq6);
    idt_set_gate(39, (unsigned int)irq7);
    idt_set_gate(40, (unsigned int)irq8);
    idt_set_gate(41, (unsigned int)irq9);
    idt_set_gate(42, (unsigned int)irq10);
    idt_set_gate(43, (unsigned int)irq11);
    idt_set_gate(44, (unsigned int)irq12);
    idt_set_gate(45, (unsigned int)irq13);
    idt_set_gate(46, (unsigned int)irq14);
    idt_set_gate(47, (unsigned int)irq15);

    // Load the IDT into the CPU register using inline assembly
    __asm__ volatile("lidt (%0)" : : "r"(&idt_p));
}
