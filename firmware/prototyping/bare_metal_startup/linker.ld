ENTRY(_reset);

MEMORY {
    flash(rx) : ORIGIN =0x08000000, LENGTH =128k
    sram(rwx) : ORIGIN =0x20000000, LENGTH =32k
}

_estack = ORIGIN(sram) + LENGTH(sram);

SECTIONS {
    .vectors : {
        . = ALIGN(4);
        KEEP(*(.vectors))
    } > flash
    .text    : {
        . = ALIGN(4);
        *(.text*)
    } > flash
    .rodata  : {
        . = ALIGN(4);
        *(.rodata*)
    } > flash

    .data : {
        . = ALIGN(4);
        _sdata = .;
        *(.data SORT(.data.*))
        . = ALIGN(4);
        _edata = .;
    } > sram AT > flash
    _sidata = LOADADDR(.data);

    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.bss SORT(.bss.*) COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > sram
}