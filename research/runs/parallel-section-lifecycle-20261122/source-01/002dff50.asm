002dff50	d0ffbd27	addiu sp,sp,-0x30
002dff54	1000bfff	sd ra,0x10(sp)
002dff58	0000b07f	sq s0,0x0(sp)
002dff5c	2d808000	move s0,a0
002dff60	0d000052	beql s0,zero,0x002dff98
002dff64	1000bfdf	_ld ra,0x10(sp)
002dff68	0000048e	lw a0,0x0(s0)
002dff6c	09008010	beq a0,zero,0x002dff94
002dff70	00000000	_nop
002dff74	789c0c0c	jal 0x003271e0
002dff78	00000000	_nop
002dff7c	0000038e	lw v1,0x0(s0)
002dff80	04006010	beq v1,zero,0x002dff94
002dff84	00000000	_nop
002dff88	2d200002	move a0,s0
002dff8c	68810b0c	jal 0x002e05a0
002dff90	2800a527	_addiu a1,sp,0x28
002dff94	1000bfdf	ld ra,0x10(sp)
002dff98	0000b07b	lq s0,0x0(sp)
002dff9c	0800e003	jr ra
002dffa0	3000bd27	_addiu sp,sp,0x30
