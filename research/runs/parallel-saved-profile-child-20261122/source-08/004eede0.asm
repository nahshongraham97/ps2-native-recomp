004eede0	e0ffbd27	addiu sp,sp,-0x20
004eede4	0000bfff	sd ra,0x0(sp)
004eede8	f416090c	jal 0x00245bd0
004eedec	1c00a527	_addiu a1,sp,0x1c
004eedf0	00004390	lbu v1,0x0(v0)
004eedf4	01006330	andi v1,v1,0x1
004eedf8	03006010	beq v1,zero,0x004eee08
004eedfc	00000000	_nop
004eee00	03000010	b 0x004eee10
004eee04	e8024224	_addiu v0,v0,0x2e8
004eee08	6400023c	lui v0,0x64
004eee0c	886e4224	addiu v0,v0,0x6e88
004eee10	0000bfdf	ld ra,0x0(sp)
004eee14	0800e003	jr ra
004eee18	2000bd27	_addiu sp,sp,0x20
