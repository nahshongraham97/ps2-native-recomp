002c3fc0	57	PUSH EDI
002c3fc1	8b7c2408	MOV EDI,dword ptr [ESP + 0x8]
002c3fc5	8b4720	MOV EAX,dword ptr [EDI + 0x20]
002c3fc8	8b4f1c	MOV ECX,dword ptr [EDI + 0x1c]
002c3fcb	8908	MOV dword ptr [EAX],ECX
002c3fcd	8b571c	MOV EDX,dword ptr [EDI + 0x1c]
002c3fd0	8b4720	MOV EAX,dword ptr [EDI + 0x20]
002c3fd3	894204	MOV dword ptr [EDX + 0x4],EAX
002c3fd6	e885ffffff	CALL 0x002c3f60
002c3fdb	b801000000	MOV EAX,0x1
002c3fe0	5f	POP EDI
002c3fe1	c3	RET
