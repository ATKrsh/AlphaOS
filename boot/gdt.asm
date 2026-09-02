; gdt.asm
; Global Descriptor Table (GDT) setup for flat 32-bit Protected Mode

gdt_start:

; 1. Null Descriptor (mandatory 8 bytes of zeroes)
gdt_null:
    dd 0x0
    dd 0x0

; 2. Code Segment Descriptor
; Base: 0x00000000, Limit: 0xfffff (translates to 4GB via Granularity flag)
; Type: Present=1, Privilege=00 (Ring 0), Descriptor type=1 (code/data),
;       Code=1, Conforming=0, Readable=1, Accessed=0 -> 10011010b
; Flags: Granularity=1 (blocks of 4KB), 32-bit default=1, 64-bit segment=0, AVL=0 -> 1100b
gdt_code:
    dw 0xffff    ; Limit (bits 0-15)
    dw 0x0       ; Base (bits 0-15)
    db 0x0       ; Base (bits 16-23)
    db 10011010b ; Access byte
    db 11001111b ; Flags (bits 0-3) + Limit (bits 16-19)
    db 0x0       ; Base (bits 24-31)

; 3. Data Segment Descriptor
; Base: 0x00000000, Limit: 0xfffff (4GB)
; Type: Present=1, Privilege=00 (Ring 0), Descriptor type=1 (code/data),
;       Code=0, Expand down=0, Writable=1, Accessed=0 -> 10010010b
; Flags: Granularity=1, 32-bit default=1 -> 1100b
gdt_data:
    dw 0xffff    ; Limit (bits 0-15)
    dw 0x0       ; Base (bits 0-15)
    db 0x0       ; Base (bits 16-23)
    db 10010010b ; Access byte
    db 11001111b ; Flags (bits 0-3) + Limit (bits 16-19)
    db 0x0       ; Base (bits 24-31)

gdt_end:

; GDT Descriptor structure for the LGDT instruction
gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; Size of GDT (always 1 byte less than true size)
    dd gdt_start               ; Address of the GDT start

; Constants for segment descriptor offsets
; The CPU expects these selector values in segment registers in Protected Mode
CODE_SEG equ gdt_code - gdt_start ; Segment selector for code (0x08)
DATA_SEG equ gdt_data - gdt_start ; Segment selector for data (0x10)
