	.syntax unified
	.cpu cortex-m3
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 1
	.eabi_attribute 30, 6
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4
	.thumb
	.syntax unified
	.file	"main.c"
	.global	BootROM_flash_erase
	.section	.data.BootROM_flash_erase,"aw",%progbits
	.align	2
	.type	BootROM_flash_erase, %object
	.size	BootROM_flash_erase, 4
BootROM_flash_erase:
	.word	509
	.section	.text.main,"ax",%progbits
	.align	2
	.global	main
	.thumb
	.thumb_func
	.type	main, %function
main:
	@ args = 0, pretend = 0, frame = 24
	@ frame_needed = 1, uses_anonymous_args = 0
	push	{r4, r7, lr}
	sub	sp, sp, #36
	add	r7, sp, #8
	mov	r3, #268439552
	str	r3, [r7, #12]
	ldr	r3, [r7, #12]
	ldr	r3, [r3, #4]
	str	r3, [r7, #20]
	ldr	r3, [r7, #12]
	ldr	r3, [r3, #8]
	str	r3, [r7, #8]
	ldr	r2, [r7, #20]
	ldr	r3, [r7, #8]
	add	r3, r3, r2
	str	r3, [r7, #4]
	ldr	r3, [r7, #20]
	cmp	r3, #16777216
	bcs	.L2
	ldr	r3, [r7, #12]
	mov	r2, #-1
	str	r2, [r3, #16]
	b	.L3
.L2:
	ldr	r3, [r7, #20]
	cmp	r3, #17301504
	bcc	.L4
	mvn	r3, #1
	b	.L5
.L4:
	ldr	r3, [r7, #20]
	mvn	r2, #-16777216
	cmp	r3, r2
	ite	ls
	movls	r3, #1
	movhi	r3, #0
	uxtb	r2, r3
	ldr	r3, [r7, #20]
	ldr	r1, .L11
	cmp	r3, r1
	ite	hi
	movhi	r3, #1
	movls	r3, #0
	uxtb	r3, r3
	orrs	r3, r3, r2
	uxtb	r3, r3
	mov	r1, r3
	ldr	r3, [r7, #4]
	ldr	r2, .L11
	cmp	r3, r2
	ite	hi
	movhi	r3, #1
	movls	r3, #0
	uxtb	r3, r3
	orrs	r3, r3, r1
	cmp	r3, #0
	beq	.L7
	ldr	r3, [r7, #12]
	mov	r2, #-1
	str	r2, [r3, #16]
	b	.L3
.L10:
	ldr	r3, [r7, #12]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	beq	.L8
	ldr	r3, [r7, #20]
	lsrs	r2, r3, #2
	ldr	r3, .L11+4
	ands	r3, r3, r2
	str	r3, [r7, #16]
	b	.L9
.L8:
	ldr	r3, [r7, #20]
	bic	r3, r3, #4080
	bic	r3, r3, #15
	str	r3, [r7, #16]
.L9:
	ldr	r3, .L11+8
	ldr	r4, [r3]
	movs	r3, #48
	str	r3, [sp, #4]
	mov	r3, #768
	str	r3, [sp]
	movs	r3, #48
	movs	r2, #16
	movs	r1, #0
	ldr	r0, [r7, #16]
	blx	r4
	ldr	r3, [r7, #20]
	add	r3, r3, #4096
	str	r3, [r7, #20]
.L7:
	ldr	r2, [r7, #20]
	ldr	r3, [r7, #4]
	cmp	r2, r3
	bcc	.L10
	ldr	r3, [r7, #12]
	movs	r2, #0
	str	r2, [r3, #16]
.L3:
	.syntax unified
@ 110 "../src/main.c" 1
	    BKPT      #0
@ 0 "" 2
	.thumb
	.syntax unified
	movs	r3, #0
.L5:
	mov	r0, r3
	adds	r7, r7, #28
	mov	sp, r7
	@ sp needed
	pop	{r4, r7, pc}
.L12:
	.align	2
.L11:
	.word	17301503
	.word	1073740800
	.word	BootROM_flash_erase
	.size	main, .-main
	.ident	"GCC: (15:5.4.1+svn241155-1) 5.4.1 20160919"
