002ffd60	b0ffbd27	addiu sp,sp,-0x50
002ffd64	4000bfff	sd ra,0x40(sp)
002ffd68	3000b37f	sq s3,0x30(sp)
002ffd6c	2000b27f	sq s2,0x20(sp)
002ffd70	2d988000	move s3,a0
002ffd74	1000b17f	sq s1,0x10(sp)
002ffd78	0000b07f	sq s0,0x0(sp)
002ffd7c	1400838c	lw v1,0x14(a0)
002ffd80	27006010	beq v1,zero,0x002ffe20
002ffd84	2d900000	_move s2,zero
002ffd88	2d880000	move s1,zero
002ffd8c	2000658e	lw a1,0x20(s3)
002ffd90	2118b100	addu v1,a1,s1
002ffd94	0000638c	lw v1,0x0(v1)
002ffd98	bc180300	dsll32 v1,v1,0x2
002ffd9c	be180300	dsrl32 v1,v1,0x2
002ffda0	feff6324	addiu v1,v1,-0x2
002ffda4	0200612c	sltiu at,v1,0x2
002ffda8	17002010	beq at,zero,0x002ffe08
002ffdac	00000000	_nop
002ffdb0	00811200	sll s0,s2,0x4
002ffdb4	2118b000	addu v1,a1,s0
002ffdb8	0400638c	lw v1,0x4(v1)
002ffdbc	02006010	beq v1,zero,0x002ffdc8
002ffdc0	2d200000	_move a0,zero
002ffdc4	0000648c	lw a0,0x0(v1)
002ffdc8	0f008010	beq a0,zero,0x002ffe08
002ffdcc	00000000	_nop
002ffdd0	0000828c	lw v0,0x0(a0)
002ffdd4	02004010	beq v0,zero,0x002ffde0
002ffdd8	00000000	_nop
002ffddc	240040ac	sw zero,0x24(v0)
002ffde0	01000224	li v0,0x1
002ffde4	f8a5060c	jal 0x001a97e0
002ffde8	540082ac	_sw v0,0x54(a0)
002ffdec	2000638e	lw v1,0x20(s3)
002ffdf0	21187000	addu v1,v1,s0
002ffdf4	0400638c	lw v1,0x4(v1)
002ffdf8	03006010	beq v1,zero,0x002ffe08
002ffdfc	00000000	_nop
002ffe00	000060ac	sw zero,0x0(v1)
002ffe04	00000000	nop
002ffe08	1400638e	lw v1,0x14(s3)
002ffe0c	01005226	addiu s2,s2,0x1
002ffe10	2b184302	sltu v1,s2,v1
002ffe14	ddff6014	bne v1,zero,0x002ffd8c
002ffe18	10003126	_addiu s1,s1,0x10
002ffe1c	00000000	nop
002ffe20	4000bfdf	ld ra,0x40(sp)
002ffe24	3000b37b	lq s3,0x30(sp)
002ffe28	2000b27b	lq s2,0x20(sp)
002ffe2c	1000b17b	lq s1,0x10(sp)
002ffe30	0000b07b	lq s0,0x0(sp)
002ffe34	0800e003	jr ra
002ffe38	5000bd27	_addiu sp,sp,0x50
