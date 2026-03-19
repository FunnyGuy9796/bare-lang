BCC := ./../bare-compiler
LD := ld
LDFLAGS := -m elf_i386 -T linker.ld

SRCS := grub.bare
OBJS := grub.o

TARGET := kernel.elf
ISO := bare.iso

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.bare
	mkdir -p $(dir $@)
	$(BCC) -b $< -o $@

run: $(TARGET)
	mkdir -p iso/boot/grub/
	cp $(TARGET) iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o $(ISO) iso/
	qemu-system-i386 -cdrom $(ISO) -vga std -display sdl

clean:
	rm -rf $(OBJS) $(TARGET) $(ISO) iso/