ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

PHDRS {
    text    PT_LOAD;
    rodata  PT_LOAD;
    data    PT_LOAD;
    dynamic PT_DYNAMIC;
}

SECTIONS {
    /* Position-independent: base at 0 */
    . = 0;

    .text : ALIGN(4) {
        *(.text)
        *(.text.*)
    } :text

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
        *(.note .note.*)
    }
}
