.thumb
.syntax unified

.equ RCC_BASE,               0x40021000
.equ GPIOA_BASE,             0x48000000

vector:
.word 0x20001000 @ sp
.word _start + 1 @ pc

.thumb_func
_busywait:
	ldr r0, =0
	ldr r1, =300000
	ldr r2, =1
	busyloop:
	add r0, r0, r2
	cmp r0, r1
	nop
	nop
	bne busyloop
	bx lr

.thumb_func
.globl _start
_start:
	ldr r0, =(RCC_BASE + 0x14) @ ahbenr
	ldr r1, =(1 << 17)
	str r1, [r0]

	ldr r0, =(GPIOA_BASE) @ moder
	ldr r1, =(1 << (4*2))
	str r1, [r0]

	loop:
	ldr r0, =(GPIOA_BASE + 0x14) @ odr
	ldr r1, =(1 << 4)
	str r1, [r0]
	bl _busywait
	ldr r0, =(GPIOA_BASE + 0x14) @ odr
	ldr r1, =0
	str r1, [r0]
	bl _busywait
	b loop
