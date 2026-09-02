; AlphaOS Bootloader
; Loaded by the BIOS at physical memory address 0x7c00

[org 0x7c00]
[bits 16]

KERNEL_OFFSET equ 0x1000 ; Memory address offset where we load the kernel

    mov [BOOT_DRIVE], dl ; BIOS stores the boot drive number in DL on startup

    ; Set up the stack securely away from our bootloader and kernel
    mov bp, 0x9000
    mov sp, bp

    ; Clear screen in 16-bit real mode and print welcome message
    mov ah, 0x00
    mov al, 0x13 ; VGA Mode 13h (320x200 256-color graphics mode)
    int 0x10

    mov bx, MSG_REAL_MODE
    call print_string

    ; Load the kernel from the disk
    call load_kernel

    ; Switch from 16-bit Real Mode to 32-bit Protected Mode
    call switch_to_pm

    jmp $ ; Hang in case we return from protected mode

; Include helper files
%include "boot/disk.asm"
%include "boot/gdt.asm"
%include "boot/switch_pm.asm"

[bits 16]
load_kernel:
    mov bx, MSG_LOAD_KERNEL
    call print_string

    mov bx, KERNEL_OFFSET ; Read sectors from disk into memory starting at KERNEL_OFFSET
    mov dh, 32            ; Number of sectors to read. 32 sectors = 16KB of kernel code space.
    mov dl, [BOOT_DRIVE]  ; Read from the boot drive
    call disk_load
    ret

[bits 32]
; This is where we arrive after switching to and initializing Protected Mode
BEGIN_PM:
    call KERNEL_OFFSET     ; Jump to the loaded kernel code entry point!
    jmp $                  ; Hang if the kernel ever exits

; Data definitions
BOOT_DRIVE      db 0
MSG_REAL_MODE   db "Starting AlphaOS Bootloader (16-bit Real Mode)...", 13, 10, 0
MSG_LOAD_KERNEL db "Loading Kernel from disk...", 13, 10, 0
MSG_PROT_MODE   db "Successfully entered 32-bit Protected Mode. Jumping to Kernel...", 0

; Boot signature
times 510-($-$$) db 0
dw 0xaa55
