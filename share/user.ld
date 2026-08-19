ENTRY(_start)

PF_X = 1;
PF_W = 2;
PF_R = 4;

PHDRS {
    text    PT_LOAD;
    rodata  PT_LOAD;
    data    PT_LOAD;
    dynamic PT_DYNAMIC;
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

    .dynsym : ALIGN(8) {
        *(.dynsym)
    } :rodata

    .dynstr : ALIGN(4) {
        *(.dynstr)
    } :rodata

    .rela.dyn : ALIGN(8) {
        *(.rela.dyn)
        *(.rela.text)
        *(.rela.rodata)
        *(.rela.data)
        *(.rela.got)
    } :rodata

    .rela.plt : ALIGN(8) {
        *(.rela.plt)
    } :rodata

    .hash : ALIGN(4) {
        *(.hash)
        *(.gnu.hash)
    } :rodata
    
    . = ALIGN(0x1000);

    .data : ALIGN(4) {
        *(.data)
        *(.data.*)
    } :data

    .got : ALIGN(8) {
        *(.got)
    } :data

    .got.plt : ALIGN(8) {
        *(.got.plt)
    } :data

    .dynamic : ALIGN(8) {
        *(.dynamic)
    } :data :dynamic
    
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