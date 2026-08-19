ENTRY(_start)

PHDRS {
    text   PT_LOAD;
    rodata PT_LOAD;
    data   PT_LOAD;
}

SECTIONS {
    . = 0x00400000;

    .text : ALIGN(4) {
        *(.text)
        *(.text.*)
    } :text

    .rodata : ALIGN(4) {
        *(.rodata)
        *(.rodata.*)
    } :rodata

    .data : ALIGN(4) {
        *(.data)
        *(.data.*)
    } :data

    .bss : ALIGN(4) {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :data

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}