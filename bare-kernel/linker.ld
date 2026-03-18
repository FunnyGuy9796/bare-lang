SECTIONS {
    . = 0x100000;
    
    .multiboot : {
        *(.multiboot)
    }

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .bss : {
        *(.bss)
    }
}