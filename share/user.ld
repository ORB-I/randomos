ENTRY(_start)

PF_X = 1;
PF_W = 2;
PF_R = 4;

PHDRS {
    text   PT_LOAD FLAGS(PF_R | PF_X);
    rodata PT_LOAD FLAGS(PF_R);
    data   PT_LOAD FLAGS(PF_R | PF_W);
}

SECTIONS {
    . = 0x00400000;

    .text : ALIGN(4) {
        *(.text)
        *(.text.*)
    } :text
    . = ALIGN(0x1000);
    .rodata : ALIGN(4) {
        *(.rodata)
        *(.rodata.*)
    } :rodata
    . = ALIGN(0x1000);
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