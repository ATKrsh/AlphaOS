; disk.asm
; Real Mode disk load and print helper functions

[bits 16]

; Reads DH sectors to ES:BX from drive DL
disk_load:
    pusha
    push dx             ; Store DX on stack so we can check number of sectors read later

    mov ah, 0x02        ; BIOS read sector function
    mov al, dh          ; Read DH sectors
    mov ch, 0x00        ; Select cylinder 0
    mov dh, 0x00        ; Select head 0
    mov cl, 0x02        ; Start reading from 2nd sector (i.e. right after boot sector)

    int 0x13            ; BIOS disk interrupt
    jc disk_error       ; Jump if error (carry flag set by BIOS)

    pop dx              ; Restore DX (DH = number of sectors requested)
    cmp al, dh          ; BIOS sets AL to the number of sectors actually read. Check if it matches DH.
    jne sectors_error
    
    popa
    ret

disk_error:
    mov bx, DISK_ERROR_MSG
    call print_string
    mov dh, ah          ; AH contains the error code
    call print_hex      ; Print error code for debugging
    jmp $

sectors_error:
    mov bx, SECTORS_ERROR_MSG
    call print_string
    jmp $

; 16-bit Print String Function
; Prints the null-terminated string pointed to by BX
print_string:
    pusha
    mov ah, 0x0e        ; BIOS teletype output
.loop:
    mov al, [bx]
    cmp al, 0
    je .done
    int 0x10
    add bx, 1
    jmp .loop
.done:
    popa
    ret

; 16-bit Print Hex Function
; Prints the value of DX in hex format (e.g. 0x1234)
print_hex:
    pusha
    mov cx, 0           ; Index variable
.hex_loop:
    cmp cx, 4           ; Loop 4 times (for 4 hex digits in a 16-bit word)
    je .end_hex
    mov ax, dx          ; Copy DX to AX
    and ax, 0x000f      ; Mask out first 3 nibbles
    add al, 0x30        ; Convert to ASCII numeric value
    cmp al, 0x39        ; Check if it is a letter (A-F)
    jle .step2
    add al, 7           ; Convert to ASCII letter value (A-F)
.step2:
    ; Get position to insert char into our template
    mov bx, HEX_OUT + 5 ; Point to end of template string
    sub bx, cx          ; Subtract index
    mov [bx], al        ; Put char there
    shr dx, 4           ; Shift DX right by 4 bits
    add cx, 1
    jmp .hex_loop
.end_hex:
    mov bx, HEX_OUT
    call print_string
    popa
    ret

; Global variables/templates
DISK_ERROR_MSG    db "Disk read error! Code: ", 0
SECTORS_ERROR_MSG db "Disk read size mismatch error!", 13, 10, 0
HEX_OUT           db "0x0000", 13, 10, 0
