// isr.c
// Implementation of C interrupt dispatching, PIC remapping, and timer initialization.

#include "isr.h"

// Array of registered interrupt handlers
isr_t interrupt_handlers[256];

// Extern port functions defined in kernel.c
extern unsigned char port_byte_in(unsigned short port);
extern void port_byte_out(unsigned short port, unsigned char data);

// Extern print functions defined in kernel.c for panics
extern void print_log(const char* str);
extern void clear_log_area();
extern void int_to_ascii(int n, char str[]);

// Registers a custom handler for an interrupt gate
void register_interrupt_handler(unsigned char n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

// Remaps the Master and Slave PICs to map hardware IRQs 0-15 to interrupts 32-47
void pic_remap() {
    port_byte_out(0x20, 0x11); // Start initialization of Master PIC
    port_byte_out(0xA0, 0x11); // Start initialization of Slave PIC
    
    port_byte_out(0x21, 0x20); // Map Master PIC vector offset to 32 (0x20)
    port_byte_out(0xA1, 0x28); // Map Slave PIC vector offset to 40 (0x28)
    
    port_byte_out(0x21, 0x04); // Master PIC cascade connection to Slave PIC at IRQ 2
    port_byte_out(0xA1, 0x02); // Slave PIC cascade identity
    
    port_byte_out(0x21, 0x01); // Master PIC 8086 mode
    port_byte_out(0xA1, 0x01); // Slave PIC 8086 mode
    
    port_byte_out(0x21, 0x00); // Clear Master PIC interrupt mask (enable all IRQs)
    port_byte_out(0xA1, 0x00); // Clear Slave PIC interrupt mask (enable all IRQs)
}

// Sends End of Interrupt (EOI) signal to the PIC
void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        port_byte_out(0xA0, 0x20); // Send EOI to Slave PIC
    }
    port_byte_out(0x20, 0x20);     // Send EOI to Master PIC
}

// Configures the Programmable Interval Timer (PIT) frequency (Channel 0)
void pit_init(unsigned int frequency) {
    // PIT base frequency is 1193182 Hz
    unsigned int divisor = 1193182 / frequency;
    
    // Command byte: Channel 0, Access LOBYTE/HIBYTE, Square Wave Mode (mode 3), 16-bit binary
    // 00 (Channel 0) | 11 (Access L/H) | 011 (Mode 3) | 0 (Binary) = 00110110b = 0x36
    port_byte_out(0x43, 0x36);
    
    // Send divisor low byte then high byte
    port_byte_out(0x40, (unsigned char)(divisor & 0xFF));
    port_byte_out(0x40, (unsigned char)((divisor >> 8) & 0xFF));
}

// Exception error messages mapped to CPU Exception numbers 0-31
static const char* exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "x87 FPU Floating-Point Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Security Exception",
    "Reserved"
};

// Exception Dispatcher - triggers CPU Halt
void isr_handler(struct registers r) {
    clear_log_area();
    print_log("\n");
    print_log("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    print_log("                          KERNEL PANIC - CPU EXCEPTION                          \n");
    print_log("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
    
    if (r.int_no < 32) {
        print_log("Exception: ");
        print_log(exception_messages[r.int_no]);
        print_log("\n");
    } else {
        print_log("Unknown Exception\n");
    }
    
    // Dump registers
    print_log("Register State Dump:\n");
    
    char int_str[16];
    int_to_ascii(r.int_no, int_str);
    print_log("  Interrupt Number: "); print_log(int_str); print_log("\n");
    
    int_to_ascii(r.err_code, int_str);
    print_log("  Error Code      : "); print_log(int_str); print_log("\n");
    
    int_to_ascii(r.eip, int_str);
    print_log("  EIP             : 0x"); print_log(int_str); print_log("\n");
    
    int_to_ascii(r.cs, int_str);
    print_log("  CS              : 0x"); print_log(int_str); print_log("\n");
    
    int_to_ascii(r.eflags, int_str);
    print_log("  EFLAGS          : 0x"); print_log(int_str); print_log("\n");
    
    print_log("\nSystem halted. Press Ctrl+Alt+R in QEMU to restart.");
    
    // Disable interrupts and halt CPU
    __asm__ volatile("cli");
    while(1) {
        __asm__ volatile("hlt");
    }
}

// Hardware Interrupt Request (IRQ) Dispatcher
void irq_handler(struct registers r) {
    // Execute registered handler if present
    if (interrupt_handlers[r.int_no] != 0) {
        isr_t handler = interrupt_handlers[r.int_no];
        handler(&r);
    }
    
    // Send EOI to PIC (IRQs are mapped from 32 onwards, so subtract 32 to get IRQ index)
    pic_send_eoi(r.int_no - 32);
}
