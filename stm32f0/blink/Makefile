.PHONY: flash debug
all: blink.elf

main.o: main.s
	arm-none-eabi-as -mcpu=cortex-m0 -mthumb -o main.o main.s

blink.elf: main.o
	arm-none-eabi-ld main.o -T memory.ld -o blink.elf

flash: blink.elf
	sudo openocd \
		-c "source [find interface/jlink.cfg]" \
		-c "transport select swd" \
		-c "source [find target/stm32f0x.cfg]" \
		-c "init; reset init; targets; halt; flash write_image erase blink.elf"

debug: blink.elf
	arm-none-eabi-gdb --eval-command="target extended-remote :3333" blink.elf
