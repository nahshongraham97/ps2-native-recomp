002c04d0	8b442404	MOV EAX,dword ptr [ESP + 0x4]
002c04d4	8b4804	MOV ECX,dword ptr [EAX + 0x4]
002c04d7	85c9	TEST ECX,ECX
002c04d9	7411	JZ 0x002c04ec
002c04db	8b480c	MOV ECX,dword ptr [EAX + 0xc]
002c04de	8b5008	MOV EDX,dword ptr [EAX + 0x8]
002c04e1	8911	MOV dword ptr [ECX],EDX
002c04e3	8b4808	MOV ECX,dword ptr [EAX + 0x8]
002c04e6	8b500c	MOV EDX,dword ptr [EAX + 0xc]
002c04e9	895104	MOV dword ptr [ECX + 0x4],EDX
002c04ec	c3	RET
