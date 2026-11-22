0019aa30	addiu sp,sp,-0x30
0019aa34	lui v0,0x6a
0019aa38	sd ra,0x20(sp)
0019aa3c	sq s1,0x10(sp)
0019aa40	sq s0,0x0(sp)
0019aa44	andi s1,a0,0xffff
0019aa48	lw v1,-0xef4(v0)
0019aa4c	beq v1,zero,0x0019aa70
0019aa50	_sra s0,a0,0x10
0019aa54	lw v0,0x0(v1)
0019aa58	bnel v0,s0,0x0019aa68
0019aa5c	_lw v1,0x18(v1)
0019aa60	b 0x0019aa74
0019aa64	_nop
0019aa68	bnel v1,zero,0x0019aa58
0019aa6c	_lw v0,0x0(v1)
0019aa70	move v1,zero
0019aa74	beq v1,zero,0x0019aaa8
0019aa78	_nop
0019aa7c	lw t0,0x14(v1)
0019aa80	lw v1,0x8(v1)
0019aa84	beq v1,zero,0x0019aaa8
0019aa88	_move a3,zero
0019aa8c	lw v0,0x0(t0)
0019aa90	beq v0,s1,0x0019aaa8
0019aa94	_nop
0019aa98	addiu a3,a3,0x1
0019aa9c	sltu v0,a3,v1
0019aaa0	bne v0,zero,0x0019aa8c
0019aaa4	_addiu t0,t0,0x18
0019aaa8	jal 0x0019b530
0019aaac	_nop
0019aab0	lui v1,0x6a
0019aab4	lw a0,-0xef4(v1)
0019aab8	beq a0,zero,0x0019aae0
0019aabc	_nop
0019aac0	lw v1,0x0(a0)
0019aac4	bnel v1,s0,0x0019aad4
0019aac8	_lw a0,0x18(a0)
0019aacc	b 0x0019aae4
0019aad0	_nop
0019aad4	bnel a0,zero,0x0019aac4
0019aad8	_lw v1,0x0(a0)
0019aadc	nop
0019aae0	move a0,zero
0019aae4	bnel a0,zero,0x0019aaf4
0019aae8	_lw a2,0x14(a0)
0019aaec	b 0x0019ab24
0019aaf0	_move a2,zero
0019aaf4	lw a0,0x8(a0)
0019aaf8	beq a0,zero,0x0019ab20
0019aafc	_move a1,zero
0019ab00	lw v1,0x0(a2)
0019ab04	bnel v1,s1,0x0019ab14
0019ab08	_addiu a1,a1,0x1
0019ab0c	b 0x0019ab24
0019ab10	_nop
0019ab14	sltu v1,a1,a0
0019ab18	bne v1,zero,0x0019ab00
0019ab1c	_addiu a2,a2,0x18
0019ab20	move a2,zero
0019ab24	beq a2,zero,0x0019abd8
0019ab28	_nop
0019ab2c	lw s0,0x10(a2)
0019ab30	beq s0,zero,0x0019abd8
0019ab34	_nop
0019ab38	beq s0,zero,0x0019abd8
0019ab3c	_nop
0019ab40	lbu v1,0x1c(s0)
0019ab44	bne v1,zero,0x0019abc8
0019ab48	_nop
0019ab4c	jal 0x003ddef0
0019ab50	_move a0,s0
0019ab54	lbu v1,0x1c(s0)
0019ab58	bne v1,zero,0x0019abc8
0019ab5c	_nop
0019ab60	lw v1,0x0(s0)
0019ab64	beq v1,zero,0x0019abc8
0019ab68	_nop
0019ab6c	lw a0,0x8(v1)
0019ab70	li v1,0x3
0019ab74	beq a0,v1,0x0019abc0
0019ab78	_nop
0019ab7c	li v1,0x1
0019ab80	beq a0,v1,0x0019abb0
0019ab84	_nop
0019ab88	li v1,0x2
0019ab8c	beq a0,v1,0x0019aba0
0019ab90	_nop
0019ab94	b 0x0019abc8
0019ab98	_nop
0019aba0	jal 0x002ffe40
0019aba4	_move a0,s0
0019aba8	b 0x0019abc8
0019abac	_nop
0019abb0	jal 0x003011d0
0019abb4	_move a0,s0
0019abb8	b 0x0019abc8
0019abbc	_nop
0019abc0	jal 0x00215120
0019abc4	_move a0,s0
0019abc8	lw s0,0x18(s0)
0019abcc	bnel s0,zero,0x0019ab44
0019abd0	_lbu v1,0x1c(s0)
0019abd4	nop
0019abd8	ld ra,0x20(sp)
0019abdc	lq s1,0x10(sp)
0019abe0	lq s0,0x0(sp)
0019abe4	jr ra
0019abe8	_addiu sp,sp,0x30
