CFLAGS := -Wl,-Tlink.lds -pie -nostdlib -ffreestanding -Os -fomit-frame-pointer

.PHONY: all clean
# Uncomment to keep .elf intermediate files for debugging purposes
# .PRECIOUS: playground-i386.elf playground-arm64.elf

all: playground-i386.bin playground-arm64.bin

%-i386.bin: %-i386.elf
	objcopy -O binary --only-section=.text $< $@

%-i386.elf: %.c link.lds
	gcc -m32 $(CFLAGS) -o $@ $<

%-arm64.bin: %-arm64.elf
	aarch64-linux-gnu-objcopy -O binary --only-section=.text $< $@

%-arm64.elf: %.c link.lds
	aarch64-linux-gnu-gcc $(CFLAGS) -o $@ $<

clean:
	rm -f *.bin *.elf
