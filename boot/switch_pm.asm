; switch_pm.asm
; Routines to transition from 16-bit Real Mode to 32-bit Protected Mode

[bits 16]
switch_to_pm:
    ; 1. Enable A20 gate via Fast A20 (System Control Port A)
    ; This enables access to memory above 1MB
    in al, 0x92
    or al, 0x02
    out 0x92, al

    ; 2. Disable interrupts
    cli

    ; 3. Load GDT descriptor
    lgdt [gdt_descriptor]

    ; 4. Set CR0 Protected Mode Enable (PE) bit
    mov eax, cr0
    or eax, 0x01
    mov cr0, eax

    ; 5. Far jump to clear CPU pipeline of 16-bit instructions and load CS with CODE_SEG
    jmp CODE_SEG:init_pm

[bits 32]
; We are now in 32-bit Protected Mode!
init_pm:
    ; 1. Initialize data segment registers to our flat GDT data selector
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 2. Setup the protected mode stack at a safe location
    mov ebp, 0x90000
    mov esp, ebp

    ; 3. Call our bootloader's Protected Mode entry point
    call BEGIN_PM


; 32-bit Protected Mode Print String Function
; Prints a null-terminated string pointed to by EBX to VGA video memory (0xB8000)
[bits 32]
VIDEO_MEMORY    equ 0xb8000
WHITE_ON_BLUE   equ 0x1f    ; white text, blue background

print_string_pm:
    pusha
    mov edx, VIDEO_MEMORY
    add edx, 80 * 2 * 2     ; Print on line 2 (offset 80 * 2 chars/line * 2 bytes/char)
.loop:
    mov al, [ebx]
    mov ah, WHITE_ON_BLUE
    cmp al, 0
    je .done
    mov [edx], ax
    add ebx, 1
    add edx, 2
    jmp .loop
.done:
    popa
    ret
