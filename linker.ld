OUTPUT_FORMAT("elf64-x86-64")

MEMORY
{
    RAM (rwx) : ORIGIN = 0x30000000, LENGTH = 64K   /* Define memory region for RAM */
    FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 1M   /* Define memory region for Flash */
}

SECTIONS
{
    /* Place .text section at the beginning of FLASH */
    .text : 
    {
        *(.text)           /* All the .text sections from all object files */
    } > FLASH

    /* Read-only data section placed in FLASH */
    .rodata :
    {
        *(.rodata)         /* All the .rodata sections from all object files */
    } > FLASH

    /* Data section placed in RAM */
    .data :
    {
        *(.data)           /* All the .data sections from all object files */
    } > RAM

    /* BSS section placed in RAM */
    .bss :
    {
        *(.bss)            /* All the .bss sections from all object files */
    } > RAM
}
