ENTRY(entry)
SECTIONS {
	. = 0x6900000;
	.text : ALIGN(4096) {
		*(.entry)
		*(.text .text.* .data .data.* .rodata .rodata.* .bss .bss.*)
	}
}
