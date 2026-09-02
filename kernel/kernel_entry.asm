; kernel_entry.asm
; Assembly stub that calls the C kernel main function.
; This is linked at the exact start address of the kernel binary (0x1000).

[bits 32]
[extern kernel_main] ; Declare external C main function

global _start
_start:
    call kernel_main ; Call our kernel main function in C
    jmp $            ; In case the C function returns, enter an infinite loop
