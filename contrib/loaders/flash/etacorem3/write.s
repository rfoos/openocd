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
	.file	"write.c"
	.comm	BootROM_flash_program,4,4
	.section	.text.main,"ax",%progbits
	.align	2
	.global	main
	.thumb
	.thumb_func
	.type	main, %function
main:
	@ args = 0, pretend = 0, frame = 56
	@ frame_needed = 1, uses_anonymous_args = 0
	push	{r4, r7, lr}
	sub	sp, sp, #76
	add	r7, sp, #16
	mov	r3, #268439552
	str	r3, [r7, #36]
	ldr	r3, [r7, #36]
	ldr	r3, [r3]
	str	r3, [r7, #32]
	ldr	r3, [r7, #36]
	ldr	r3, [r3, #4]
	str	r3, [r7, #28]
	ldr	r2, [r7, #32]
	ldr	r3, [r7, #28]
	add	r3, r3, r2
	str	r3, [r7, #24]
	ldr	r3, [r7, #36]
	ldr	r3, [r3, #8]
	str	r3, [r7, #52]
	ldr	r3, [r7, #28]
	cmp	r3, #0
	bne	.L2
	ldr	r3, [r7, #36]
	movs	r2, #0
	str	r2, [r3, #20]
	b	.L3
.L2:
	ldr	r3, [r7, #52]
	cmp	r3, #0
	bne	.L4
	ldr	r3, .L20
	str	r3, [r7, #52]
.L4:
	ldr	r3, [r7, #32]
	cmp	r3, #16777216
	bcs	.L5
	ldr	r3, [r7, #36]
	mov	r2, #-1
	str	r2, [r3, #20]
	b	.L3
.L5:
	ldr	r3, [r7, #32]
	cmp	r3, #17301504
	bcc	.L6
	ldr	r3, [r7, #36]
	mvn	r2, #1
	str	r2, [r3, #20]
	b	.L3
.L6:
	ldr	r3, [r7, #24]
	cmp	r3, #17301504
	bls	.L7
	ldr	r3, [r7, #36]
	mvn	r2, #2
	str	r2, [r3, #20]
	b	.L3
.L7:
	ldr	r3, [r7, #28]
	cmp	r3, #4096
	bls	.L8
	ldr	r3, [r7, #36]
	mvn	r2, #3
	str	r2, [r3, #20]
	b	.L3
.L8:
	ldr	r3, [r7, #36]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	beq	.L9
	ldr	r3, [r7, #36]
	ldr	r3, [r3, #12]
	mov	r2, r3
	ldr	r3, .L20+4
	str	r2, [r3]
	b	.L10
.L9:
	ldr	r3, .L20+4
	movw	r2, #641
	str	r2, [r3]
.L10:
	ldr	r3, [r7, #28]
	lsrs	r3, r3, #2
	str	r3, [r7, #20]
	ldr	r3, [r7, #36]
	ldr	r3, [r3, #16]
	cmp	r3, #1
	bne	.L11
	movs	r3, #128
	str	r3, [r7, #16]
	mov	r3, #512
	str	r3, [r7, #12]
	ldr	r3, [r7, #20]
	ldr	r2, [r7, #16]
	udiv	r2, r3, r2
	ldr	r1, [r7, #16]
	mul	r2, r1, r2
	subs	r3, r3, r2
	str	r3, [r7, #8]
	ldr	r2, [r7, #20]
	ldr	r3, [r7, #16]
	udiv	r2, r2, r3
	ldr	r3, [r7, #8]
	cmp	r3, #0
	beq	.L12
	movs	r3, #1
	b	.L13
.L12:
	movs	r3, #0
.L13:
	add	r3, r3, r2
	str	r3, [r7, #4]
	ldr	r3, [r7, #32]
	str	r3, [r7, #48]
	ldr	r3, [r7, #52]
	str	r3, [r7, #44]
	movs	r3, #0
	str	r3, [r7, #40]
	b	.L14
.L17:
	ldr	r3, [r7, #8]
	cmp	r3, #0
	beq	.L15
	ldr	r3, [r7, #4]
	subs	r2, r3, #1
	ldr	r3, [r7, #40]
	cmp	r2, r3
	bne	.L15
	ldr	r3, .L20+4
	ldr	r4, [r3]
	movs	r3, #48
	str	r3, [sp, #12]
	mov	r3, #768
	str	r3, [sp, #8]
	movs	r3, #80
	str	r3, [sp, #4]
	movs	r3, #40
	str	r3, [sp]
	movs	r3, #16
	ldr	r2, [r7, #8]
	ldr	r1, [r7, #44]
	ldr	r0, [r7, #48]
	blx	r4
	b	.L16
.L15:
	ldr	r3, .L20+4
	ldr	r4, [r3]
	movs	r3, #48
	str	r3, [sp, #12]
	mov	r3, #768
	str	r3, [sp, #8]
	movs	r3, #80
	str	r3, [sp, #4]
	movs	r3, #40
	str	r3, [sp]
	movs	r3, #16
	ldr	r2, [r7, #16]
	ldr	r1, [r7, #44]
	ldr	r0, [r7, #48]
	blx	r4
.L16:
	ldr	r2, [r7, #48]
	ldr	r3, [r7, #12]
	add	r3, r3, r2
	str	r3, [r7, #48]
	ldr	r3, [r7, #16]
	lsls	r3, r3, #2
	ldr	r2, [r7, #44]
	add	r3, r3, r2
	str	r3, [r7, #44]
	ldr	r3, [r7, #40]
	adds	r3, r3, #1
	str	r3, [r7, #40]
.L14:
	ldr	r2, [r7, #40]
	ldr	r3, [r7, #4]
	cmp	r2, r3
	bcc	.L17
	b	.L18
.L11:
	ldr	r3, .L20+4
	ldr	r4, [r3]
	ldr	r1, [r7, #52]
	movs	r3, #48
	str	r3, [sp, #12]
	mov	r3, #768
	str	r3, [sp, #8]
	movs	r3, #80
	str	r3, [sp, #4]
	movs	r3, #40
	str	r3, [sp]
	movs	r3, #16
	ldr	r2, [r7, #20]
	ldr	r0, [r7, #32]
	blx	r4
.L18:
	ldr	r3, [r7, #36]
	movs	r2, #0
	str	r2, [r3, #20]
.L3:
	.syntax unified
@ 151 "../src/write.c" 1
	    BKPT      #0
@ 0 "" 2
	.thumb
	.syntax unified
	movs	r3, #0
	mov	r0, r3
	adds	r7, r7, #60
	mov	sp, r7
	@ sp needed
	pop	{r4, r7, pc}
.L21:
	.align	2
.L20:
	.word	268443648
	.word	BootROM_flash_program
	.size	main, .-main
	.ident	"GCC: (15:5.4.1+svn241155-1) 5.4.1 20160919"
