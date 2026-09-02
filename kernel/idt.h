// idt.h
// Structures and function declarations for the Interrupt Descriptor Table (IDT)

#ifndef IDT_H
#define IDT_H

// Segment selector for the kernel code segment defined in our GDT (0x08)
#define KERNEL_CS 0x08

// IDT Entry (Gate Descriptor) structure
struct idt_entry {
    unsigned short low_offset;  // Lower 16 bits of handler address
    unsigned short sel;         // Kernel code segment selector (0x08)
    unsigned char always0;      // Always 0
    unsigned char flags;        // Gate flags/attributes (0x8E for 32-bit Interrupt Gate)
    unsigned short high_offset; // Higher 16 bits of handler address
} __attribute__((packed));

// IDT Pointer structure passed to the `lidt` instruction
struct idt_ptr {
    unsigned short limit;       // Size of the IDT - 1
    unsigned int base;          // Address of the IDT array
} __attribute__((packed));

// Function declarations
void idt_set_gate(int n, unsigned int handler);
void idt_install();

#endif
