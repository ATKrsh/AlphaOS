; interrupt.asm
; Low-level interrupt service routine wrappers for AlphaOS.
; Saves registers, redirects execution to C handlers, and restores state.

[bits 32]

; Macros to generate ISR/IRQ entry points
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    push 0      ; Push dummy error code
    push %1     ; Push interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    ; CPU already pushed error code
    push %1     ; Push interrupt number
    jmp isr_common_stub
%endmacro

%macro IRQ 2
  global irq%1
  irq%1:
    push 0      ; Push dummy error code
    push %2     ; Push mapped interrupt number (32-47)
    jmp irq_common_stub
%endmacro

; Define Interrupt Service Routines (ISRs) for CPU Exceptions
ISR_NOERRCODE 0   ; Division By Zero
ISR_NOERRCODE 1   ; Debug Exception
ISR_NOERRCODE 2   ; Non Maskable Interrupt
ISR_NOERRCODE 3   ; Breakpoint Exception
ISR_NOERRCODE 4   ; Into Detected Overflow
ISR_NOERRCODE 5   ; Out of Bounds Exception
ISR_NOERRCODE 6   ; Invalid Opcode Exception
ISR_NOERRCODE 7   ; No Coprocessor Exception
ISR_ERRCODE   8   ; Double Fault (pushes error code)
ISR_NOERRCODE 9   ; Coprocessor Segment Overrun
ISR_ERRCODE   10  ; Bad TSS (pushes error code)
ISR_ERRCODE   11  ; Segment Not Present (pushes error code)
ISR_ERRCODE   12  ; Stack Fault (pushes error code)
ISR_ERRCODE   13  ; General Protection Fault (pushes error code)
ISR_ERRCODE   14  ; Page Fault (pushes error code)
ISR_NOERRCODE 15  ; Unknown Interrupt Exception
ISR_NOERRCODE 16  ; Coprocessor Fault
ISR_ERRCODE   17  ; Alignment Check (pushes error code)
ISR_NOERRCODE 18  ; Machine Check
ISR_NOERRCODE 19  ; SIMD Floating Point Exception
ISR_NOERRCODE 20  ; Virtualization Exception
ISR_NOERRCODE 21  ; Reserved
ISR_NOERRCODE 22  ; Reserved
ISR_NOERRCODE 23  ; Reserved
ISR_NOERRCODE 24  ; Reserved
ISR_NOERRCODE 25  ; Reserved
ISR_NOERRCODE 26  ; Reserved
ISR_NOERRCODE 27  ; Reserved
ISR_NOERRCODE 28  ; Reserved
ISR_NOERRCODE 29  ; Reserved
ISR_ERRCODE   30  ; Security Exception (pushes error code)
ISR_NOERRCODE 31  ; Reserved

; Define Hardware Interrupt Service Routines (IRQs 0-15 mapped to 32-47)
IRQ 0,  32        ; Timer
IRQ 1,  33        ; Keyboard
IRQ 2,  34        ; Cascade (used internally)
IRQ 3,  35        ; COM2
IRQ 4,  36        ; COM1
IRQ 5,  37        ; LPT2
IRQ 6,  38        ; Floppy Disk
IRQ 7,  39        ; LPT1
IRQ 8,  40        ; CMOS Real Time Clock
IRQ 9,  41        ; Free / ACPI
IRQ 10, 42        ; Free / SCSI / NIC
IRQ 11, 43        ; Free / SCSI / NIC
IRQ 12, 44        ; PS2 Mouse
IRQ 13, 45        ; Coprocessor
IRQ 14, 46        ; Primary ATA Hard Disk
IRQ 15, 47        ; Secondary ATA Hard Disk

; Extern C functions
[extern isr_handler]
[extern irq_handler]

; Common Exception Entry Point
isr_common_stub:
    pusha           ; Pushes edi, esi, ebp, esp, ebx, edx, ecx, eax

    mov ax, ds      ; Push data segment
    push eax

    mov ax, 0x10    ; Load kernel data segment selector (0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call isr_handler

    pop eax         ; Restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa            ; Pops edi, esi, ebp, ...
    add esp, 8      ; Cleans up pushed error code and interrupt number
    iret            ; Returns from interrupt

; Common Hardware Interrupt Entry Point
irq_common_stub:
    pusha           ; Pushes edi, ...

    mov ax, ds      ; Push data segment
    push eax

    mov ax, 0x10    ; Load kernel data segment selector (0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call irq_handler

    pop eax         ; Restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa            ; Pops edi, ...
    add esp, 8      ; Cleans up pushed error code and interrupt number
    iret            ; Returns from interrupt
