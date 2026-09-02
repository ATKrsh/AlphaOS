/* linker.ld
   Linker script for the AlphaOS kernel.
   Positions the kernel at memory address 0x1000. */

ENTRY(_start)

SECTIONS
{
    /* The kernel code is loaded at physical address 0x1000 */
    . = 0x1000;

    /* The code section (.text) comes first */
    .text :
    {
        *(.text)
    }

    /* Read-only data section (constants, string literals) */
    .rodata :
    {
        *(.rodata)
    }

    /* Initialized data section */
    .data :
    {
        *(.data)
    }

    /* Uninitialized data section (BSS) and common variables */
    .bss :
    {
        *(COMMON)
        *(.bss)
    }
}
