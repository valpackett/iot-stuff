MEMORY
{
    rom(RX)     : ORIGIN = 0x08000000, LENGTH = 16K
    ram(WAIL)   : ORIGIN = 0x20000000, LENGTH = 4K
}

REGION_ALIAS("vectors", rom);

SECTIONS
{
    .vector : ALIGN(4)
    {
        FILL(0xff)

        KEEP(*(.isr_vector))
        KEEP(*(.isr_vector_nvic))
    } > vectors

    .text : ALIGN(4)
    {
        FILL(0xff)
        *(.text*)
        *(.rodata .rodata.*)
    } > rom

    .data : ALIGN(4)
    {
        _data = .;

        *(SORT_BY_ALIGNMENT(.data*))
        . = ALIGN(4);

        _edata = .;
    } > ram AT>rom = 0xff

    .bss : ALIGN(4)
    {
        _bss = .;

        *(.bss*)
        *(COMMON)
        . = ALIGN(4);

        _ebss = .;

        . += 4;

        __STACK_LIMIT = .;

        . += 4;

        _eglobals = .;
    } > ram
}
