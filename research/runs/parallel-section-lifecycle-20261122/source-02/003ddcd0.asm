003ddcd0	b0ffbd27	addiu sp,sp,-0x50
003ddcd4	4000bfff	sd ra,0x40(sp)
003ddcd8	3000b37f	sq s3,0x30(sp)
003ddcdc	2000b27f	sq s2,0x20(sp)
003ddce0	1000b17f	sq s1,0x10(sp)
003ddce4	0000b07f	sq s0,0x0(sp)
003ddce8	1400928c	lw s2,0x14(a0)
003ddcec	76004012	beq s2,zero,0x003ddec8
003ddcf0	1800918c	_lw s1,0x18(a0)
003ddcf4	0800838c	lw v1,0x8(a0)
003ddcf8	04006054	bnel v1,zero,0x003ddd0c
003ddcfc	0400438e	_lw v1,0x4(s2)
003ddd00	72000010	b 0x003ddecc
003ddd04	4000bfdf	_ld ra,0x40(sp)
003ddd0c	07006010	beq v1,zero,0x003ddd2c
003ddd10	2d280000	_move a1,zero
003ddd14	0800648c	lw a0,0x8(v1)
003ddd18	05008050	beql a0,zero,0x003ddd30
003ddd1c	ff07a424	_addiu a0,a1,0x7ff
003ddd20	40bb090c	jal 0x0026ed00
003ddd24	00000000	_nop
003ddd28	2d284000	move a1,v0
003ddd2c	ff07a424	addiu a0,a1,0x7ff
003ddd30	00f80324	li v1,-0x800
003ddd34	0800258e	lw a1,0x8(s1)
003ddd38	24808300	and s0,a0,v1
003ddd3c	0400238e	lw v1,0x4(s1)
003ddd40	0800448e	lw a0,0x8(s2)
003ddd44	3c290500	dsll32 a1,a1,0x4
003ddd48	3e290500	dsrl32 a1,a1,0x4
003ddd4c	21186500	addu v1,v1,a1
003ddd50	23186400	subu v1,v1,a0
003ddd54	2a087000	slt at,v1,s0
003ddd58	28002050	beql at,zero,0x003dddfc
003ddd5c	0400538e	_lw s3,0x4(s2)
003ddd60	0b008050	beql a0,zero,0x003ddd90
003ddd64	0400508e	_lw s0,0x4(s2)
003ddd68	1800458e	lw a1,0x18(s2)
003ddd6c	0700a010	beq a1,zero,0x003ddd8c
003ddd70	00000000	_nop
003ddd74	0800a38c	lw v1,0x8(a1)
003ddd78	04006014	bne v1,zero,0x003ddd8c
003ddd7c	00000000	_nop
003ddd80	0c00438e	lw v1,0xc(s2)
003ddd84	21188300	addu v1,a0,v1
003ddd88	0800a3ac	sw v1,0x8(a1)
003ddd8c	0400508e	lw s0,0x4(s2)
003ddd90	4d000012	beq s0,zero,0x003ddec8
003ddd94	00000000	_nop
003ddd98	0800048e	lw a0,0x8(s0)
003ddd9c	05008050	beql a0,zero,0x003dddb4
003ddda0	c4b0838f	_lw v1,-0x4f3c(gp)
003ddda4	54bb090c	jal 0x0026ed50
003ddda8	00000000	_nop
003dddac	080000ae	sw zero,0x8(s0)
003dddb0	c4b0838f	lw v1,-0x4f3c(gp)
003dddb4	04007054	bnel v1,s0,0x003dddc8
003dddb8	0400038e	_lw v1,0x4(s0)
003dddbc	cc780f0c	jal 0x003de330
003dddc0	c4b080af	_sw zero,-0x4f3c(gp)
003dddc4	0400038e	lw v1,0x4(s0)
003dddc8	3f006014	bne v1,zero,0x003ddec8
003dddcc	00000000	_nop
003dddd0	2d200002	move a0,s0
003dddd4	2d280000	move a1,zero
003dddd8	2c000624	li a2,0x2c
003ddddc	986e070c	jal 0x001dba60
003ddde0	040040ae	_sw zero,0x4(s2)
003ddde4	c0b0838f	lw v1,-0x4f40(gp)
003ddde8	000003ae	sw v1,0x0(s0)
003dddec	c0b090af	sw s0,-0x4f40(gp)
003dddf0	35000010	b 0x003ddec8
003dddf4	00000000	_nop
003dddfc	09006052	beql s3,zero,0x003dde24
003dde00	0800428e	_lw v0,0x8(s2)
003dde04	0800678e	lw a3,0x8(s3)
003dde08	0500e010	beq a3,zero,0x003dde20
003dde0c	00000000	_nop
003dde10	01000524	li a1,0x1
003dde14	2cbb090c	jal 0x0026ecb0
003dde18	2d300002	_move a2,s0
003dde1c	040062ae	sw v0,0x4(s3)
003dde20	0800428e	lw v0,0x8(s2)
003dde24	2d202002	move a0,s1
003dde28	4c73060c	jal 0x0019cd30
003dde2c	21285000	_addu a1,v0,s0
003dde30	0c0050ae	sw s0,0xc(s2)
003dde34	0800458e	lw a1,0x8(s2)
003dde38	0b00a050	beql a1,zero,0x003dde68
003dde3c	0400508e	_lw s0,0x4(s2)
003dde40	1800448e	lw a0,0x18(s2)
003dde44	07008010	beq a0,zero,0x003dde64
003dde48	00000000	_nop
003dde4c	0800838c	lw v1,0x8(a0)
003dde50	04006014	bne v1,zero,0x003dde64
003dde54	00000000	_nop
003dde58	0c00438e	lw v1,0xc(s2)
003dde5c	2118a300	addu v1,a1,v1
003dde60	080083ac	sw v1,0x8(a0)
003dde64	0400508e	lw s0,0x4(s2)
003dde68	17000012	beq s0,zero,0x003ddec8
003dde6c	00000000	_nop
003dde70	0800048e	lw a0,0x8(s0)
003dde74	05008050	beql a0,zero,0x003dde8c
003dde78	c4b0838f	_lw v1,-0x4f3c(gp)
003dde7c	54bb090c	jal 0x0026ed50
003dde80	00000000	_nop
003dde84	080000ae	sw zero,0x8(s0)
003dde88	c4b0838f	lw v1,-0x4f3c(gp)
003dde8c	04007054	bnel v1,s0,0x003ddea0
003dde90	0400038e	_lw v1,0x4(s0)
003dde94	cc780f0c	jal 0x003de330
003dde98	c4b080af	_sw zero,-0x4f3c(gp)
003dde9c	0400038e	lw v1,0x4(s0)
003ddea0	09006014	bne v1,zero,0x003ddec8
003ddea4	00000000	_nop
003ddea8	2d200002	move a0,s0
003ddeac	2d280000	move a1,zero
003ddeb0	2c000624	li a2,0x2c
003ddeb4	986e070c	jal 0x001dba60
003ddeb8	040040ae	_sw zero,0x4(s2)
003ddebc	c0b0838f	lw v1,-0x4f40(gp)
003ddec0	000003ae	sw v1,0x0(s0)
003ddec4	c0b090af	sw s0,-0x4f40(gp)
003ddec8	4000bfdf	ld ra,0x40(sp)
003ddecc	3000b37b	lq s3,0x30(sp)
003dded0	2000b27b	lq s2,0x20(sp)
003dded4	1000b17b	lq s1,0x10(sp)
003dded8	0000b07b	lq s0,0x0(sp)
003ddedc	0800e003	jr ra
003ddee0	5000bd27	_addiu sp,sp,0x50
