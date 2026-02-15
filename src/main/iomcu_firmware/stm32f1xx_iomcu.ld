/*
 * STM32F103RBT6 Linker Script for IOMCU Firmware
 *
 * Memory Layout:
 * - Bootloader: 0x08000000 - 0x08001000 (4KB)
 * - Application: 0x08001000 - 0x08020000 (124KB)
 * - RAM: 0x20000000 - 0x20005000 (20KB)
 */

MEMORY
{
    FLASH (rx) : ORIGIN = 0x08001000, LENGTH = 124K
    RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 20K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    _sidata = LOADADDR(.data);

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    .heap :
    {
        . = ALIGN(4);
        _heap_start = .;
        . = . + 0x1000; /* 4KB heap */
        _heap_end = .;
    } > RAM

    .stack :
    {
        . = ALIGN(8);
        . = . + 0x800; /* 2KB stack */
        _estack = .;
    } > RAM

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}
