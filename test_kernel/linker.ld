SECTIONS {
    . = 0x7c00;
    .text : { *(.text) }
    .rodata : { *(.rodata) }
    . = 0x7c00 + 510;
    .bootsig : { *(.bootsig) }
}