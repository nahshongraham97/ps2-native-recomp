003ddef0	d0ffbd27	addiu sp,sp,-0x30
003ddef4	2000bfff	sd ra,0x20(sp)
003ddef8	1000b17f	sq s1,0x10(sp)
003ddefc	0000b07f	sq s0,0x0(sp)
003ddf00	0400908c	lw s0,0x4(a0)
003ddf04	2e000012	beq s0,zero,0x003ddfc0
003ddf08	2d888000	__move s1,a0
003ddf0c	0c000392	lbu v1,0xc(s0)
003ddf10	2b006010	beq v1,zero,0x003ddfc0
003ddf14	00000000	__nop
003ddf18	20000010	b 0x003ddf9c
003ddf1c	2b181000	__sltu v1,zero,s0
003ddf20	0d006010	beq v1,zero,0x003ddf58
003ddf24	00000000	__nop
003ddf28	f000628c	lw v0,0xf0(v1)
003ddf2c	0a004010	beq v0,zero,0x003ddf58
003ddf30	00000000	__nop
003ddf34	803f023c	lui v0,0x3f80
003ddf38	689e82af	sw v0,-0x6198(gp)
003ddf3c	0000628c	lw v0,0x0(v1)
003ddf40	1400428c	lw v0,0x14(v0)
003ddf44	09f84000	jalr v0
003ddf48	00000000	__nop
003ddf4c	04000010	b 0x003ddf60
003ddf50	00000000	__nop
003ddf54	00000000	nop
003ddf58	907f0b0c	jal 0x002dfe40
003ddf5c	00000000	__nop
003ddf60	1d002392	lbu v1,0x1d(s1)
003ddf64	04006010	beq v1,zero,0x003ddf78
003ddf68	00000000	__nop
003ddf6c	0a000010	b 0x003ddf98
003ddf70	2d800000	__move s0,zero
003ddf74	00000000	nop
003ddf78	0400238e	lw v1,0x4(s1)
003ddf7c	04000316	bne s0,v1,0x003ddf90
003ddf80	00000000	__nop
003ddf84	1400038e	lw v1,0x14(s0)
003ddf88	03007110	beq v1,s1,0x003ddf98
003ddf8c	00000000	__nop
003ddf90	13000010	b 0x003ddfe0
003ddf94	2000bfdf	__ld ra,0x20(sp)
003ddf98	2b181000	sltu v1,zero,s0
003ddf9c	01006338	xori v1,v1,0x1
003ddfa0	07006014	bne v1,zero,0x003ddfc0
003ddfa4	00000000	__nop
003ddfa8	0c000392	lbu v1,0xc(s0)
003ddfac	04006010	beq v1,zero,0x003ddfc0
003ddfb0	00000000	__nop
003ddfb4	0400038e	lw v1,0x4(s0)
003ddfb8	d9ff6050	beql v1,zero,0x003ddf20
003ddfbc	649e838f	__lw v1,-0x619c(gp)
003ddfc0	06000012	beq s0,zero,0x003ddfdc
003ddfc4	00000000	__nop
003ddfc8	0400038e	lw v1,0x4(s0)
003ddfcc	03006010	beq v1,zero,0x003ddfdc
003ddfd0	00000000	__nop
003ddfd4	9c7f0b0c	jal 0x002dfe70
003ddfd8	04000426	__addiu a0,s0,0x4
003ddfdc	2000bfdf	ld ra,0x20(sp)
003ddfe0	1000b17b	lq s1,0x10(sp)
003ddfe4	0000b07b	lq s0,0x0(sp)
003ddfe8	0800e003	jr ra
003ddfec	3000bd27	__addiu sp,sp,0x30
