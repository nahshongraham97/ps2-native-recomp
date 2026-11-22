003ddc90	f0ffbd27	addiu sp,sp,-0x10
003ddc94	0000bfff	sd ra,0x0(sp)
003ddc98	e89b828f	lw v0,-0x6418(gp)
003ddc9c	02004014	bne v0,zero,0x003ddca8
003ddca0	00000000	_nop
003ddca4	2d480000	move t1,zero
003ddca8	3e000b3c	lui t3,0x3e
003ddcac	01000a24	li t2,0x1
003ddcb0	84780f0c	jal 0x003de210
003ddcb4	d0dc6b25	_addiu t3,t3,-0x2330
003ddcb8	0000bfdf	ld ra,0x0(sp)
003ddcbc	01000224	li v0,0x1
003ddcc0	0800e003	jr ra
003ddcc4	1000bd27	_addiu sp,sp,0x10
