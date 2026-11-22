0017ece0	e0ffbd27	addiu sp,sp,-0x20
0017ece4	1000bfff	sd ra,0x10(sp)
0017ece8	0000b07f	sq s0,0x0(sp)
0017ecec	2d800000	move s0,zero
0017ecf0	50fb050c	jal 0x0017ed40
0017ecf4	2d200002	_move a0,s0
0017ecf8	01001026	addiu s0,s0,0x1
0017ecfc	0400032a	slti v1,s0,0x4
0017ed00	00000000	nop
0017ed04	faff6014	bne v1,zero,0x0017ecf0
0017ed08	00000000	_nop
0017ed0c	10a1848f	lw a0,-0x5ef0(gp)
0017ed10	06008050	beql a0,zero,0x0017ed2c
0017ed14	1000bfdf	_ld ra,0x10(sp)
0017ed18	01000524	li a1,0x1
0017ed1c	8cfb050c	jal 0x0017ee30
0017ed20	2d300000	_move a2,zero
0017ed24	10a180af	sw zero,-0x5ef0(gp)
0017ed28	1000bfdf	ld ra,0x10(sp)
0017ed2c	0000b07b	lq s0,0x0(sp)
0017ed30	0800e003	jr ra
0017ed34	2000bd27	_addiu sp,sp,0x20
