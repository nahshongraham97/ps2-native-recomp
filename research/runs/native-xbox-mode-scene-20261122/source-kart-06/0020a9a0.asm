0020a9a0	55	PUSH EBP
0020a9a1	8bec	MOV EBP,ESP
0020a9a3	83ec1c	SUB ESP,0x1c
0020a9a6	894de8	MOV dword ptr [EBP + -0x18],ECX
0020a9a9	8b4508	MOV EAX,dword ptr [EBP + 0x8]
0020a9ac	8945e4	MOV dword ptr [EBP + -0x1c],EAX
0020a9af	817de4540e0100	CMP dword ptr [EBP + -0x1c],0x10e54
0020a9b6	0f8ca5000000	JL 0x0020aa61
0020a9bc	817de4570e0100	CMP dword ptr [EBP + -0x1c],0x10e57
0020a9c3	7e05	JLE 0x0020a9ca
0020a9c5	e997000000	JMP 0x0020aa61
0020a9ca	e851a6efff	CALL 0x00105020
0020a9cf	8945ec	MOV dword ptr [EBP + -0x14],EAX
0020a9d2	8b4d08	MOV ECX,dword ptr [EBP + 0x8]
0020a9d5	81e9540e0100	SUB ECX,0x10e54
0020a9db	894df4	MOV dword ptr [EBP + -0xc],ECX
0020a9de	8b55f4	MOV EDX,dword ptr [EBP + -0xc]
0020a9e1	6bd20c	IMUL EDX,EDX,0xc
0020a9e4	81c2a0ec8200	ADD EDX,0x82eca0
0020a9ea	8955f8	MOV dword ptr [EBP + -0x8],EDX
0020a9ed	8b45f4	MOV EAX,dword ptr [EBP + -0xc]
0020a9f0	6bc00c	IMUL EAX,EAX,0xc
0020a9f3	05a4ec8200	ADD EAX,0x82eca4
0020a9f8	8945f0	MOV dword ptr [EBP + -0x10],EAX
0020a9fb	8b4d0c	MOV ECX,dword ptr [EBP + 0xc]
0020a9fe	8b510c	MOV EDX,dword ptr [ECX + 0xc]
0020aa01	8955fc	MOV dword ptr [EBP + -0x4],EDX
0020aa04	8b45fc	MOV EAX,dword ptr [EBP + -0x4]
0020aa07	83c001	ADD EAX,0x1
0020aa0a	8945fc	MOV dword ptr [EBP + -0x4],EAX
0020aa0d	8b4dec	MOV ECX,dword ptr [EBP + -0x14]
0020aa10	8b55fc	MOV EDX,dword ptr [EBP + -0x4]
0020aa13	3b5130	CMP EDX,dword ptr [ECX + 0x30]
0020aa16	7c07	JL 0x0020aa1f
0020aa18	c745fc00000000	MOV dword ptr [EBP + -0x4],0x0
0020aa1f	8b45ec	MOV EAX,dword ptr [EBP + -0x14]
0020aa22	8b4dfc	MOV ECX,dword ptr [EBP + -0x4]
0020aa25	3b4830	CMP ECX,dword ptr [EAX + 0x30]
0020aa28	7d1b	JGE 0x0020aa45
0020aa2a	8b55f8	MOV EDX,dword ptr [EBP + -0x8]
0020aa2d	8b45fc	MOV EAX,dword ptr [EBP + -0x4]
0020aa30	8b4dec	MOV ECX,dword ptr [EBP + -0x14]
0020aa33	8b448108	MOV EAX,dword ptr [ECX + EAX*0x4 + 0x8]
0020aa37	8902	MOV dword ptr [EDX],EAX
0020aa39	8b4df0	MOV ECX,dword ptr [EBP + -0x10]
0020aa3c	8b55f8	MOV EDX,dword ptr [EBP + -0x8]
0020aa3f	8b02	MOV EAX,dword ptr [EDX]
0020aa41	8901	MOV dword ptr [ECX],EAX
0020aa43	eb0b	JMP 0x0020aa50
0020aa45	8b4df8	MOV ECX,dword ptr [EBP + -0x8]
0020aa48	8b55ec	MOV EDX,dword ptr [EBP + -0x14]
0020aa4b	8b4208	MOV EAX,dword ptr [EDX + 0x8]
0020aa4e	8901	MOV dword ptr [ECX],EAX
0020aa50	8b4df8	MOV ECX,dword ptr [EBP + -0x8]
0020aa53	83390a	CMP dword ptr [ECX],0xa
0020aa56	7209	JC 0x0020aa61
0020aa58	8b55f8	MOV EDX,dword ptr [EBP + -0x8]
0020aa5b	c70200000000	MOV dword ptr [EDX],0x0
0020aa61	8be5	MOV ESP,EBP
0020aa63	5d	POP EBP
0020aa64	c20c00	RET 0xc
