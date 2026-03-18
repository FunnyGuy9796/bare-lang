BCC := ./../bare-compiler
LD := ld
LDFLAGS := -m elf_i386 -T linker.ld

SRCS := $(shell find -name "*.bare")
OBJS := $(patsubst %.bare, %.o, $(SRCS))

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
	qemu-system-i386 -cdrom $(ISO)

clean:
	rm -rf $(OBJS) $(TARGET) $(ISO) iso/