
void FUN_00514700(void)

{
  undefined1 uVar1;
  undefined1 uVar2;
  uint uVar3;
  int iVar4;
  int iVar5;
  uint uVar6;
  
  FUN_00514d30();
  uRam007bfbdc = uRam007c25ec;
  uRam007bfc24 = uRam007c25d4;
  uRam007bfc2c = uRam007c25dc;
  FUN_001db8a8(0x7bd230,0x7bfc40,0x29f4);
  uVar6 = 0;
  uRam007bfc20 = uRam007c2630;
  if (uRam007c2630 != 0) {
    uVar3 = uRam007c2630 - 8;
    if (8 < uRam007c2630) {
      iVar5 = 0x7bfc40;
      iVar4 = 0x7bd230;
      do {
        uVar6 = uVar6 + 8;
        uVar1 = *(undefined1 *)(iVar5 + 0x29b5);
        uVar2 = *(undefined1 *)(iVar5 + 0x29b6);
        *(undefined1 *)(iVar4 + 0x29b4) = *(undefined1 *)(iVar5 + 0x29b4);
        *(undefined1 *)(iVar4 + 0x29b5) = uVar1;
        *(undefined1 *)(iVar4 + 0x29b6) = uVar2;
        uVar1 = *(undefined1 *)(iVar5 + 0x29b8);
        uVar2 = *(undefined1 *)(iVar5 + 0x29b9);
        *(undefined1 *)(iVar4 + 0x29b7) = *(undefined1 *)(iVar5 + 0x29b7);
        *(undefined1 *)(iVar4 + 0x29b8) = uVar1;
        *(undefined1 *)(iVar4 + 0x29b9) = uVar2;
        uVar1 = *(undefined1 *)(iVar5 + 0x29bb);
        *(undefined1 *)(iVar4 + 0x29ba) = *(undefined1 *)(iVar5 + 0x29ba);
        *(undefined1 *)(iVar4 + 0x29bb) = uVar1;
        iVar5 = iVar5 + 8;
        iVar4 = iVar4 + 8;
      } while (uVar6 < uVar3);
    }
    uVar3 = uRam007c2630;
    if (uVar6 < uRam007c2630) {
      iVar5 = uVar6 + 0x7bfc40;
      iVar4 = uVar6 + 0x7bd230;
      do {
        uVar6 = uVar6 + 1;
        *(undefined1 *)(iVar4 + 0x29b4) = *(undefined1 *)(iVar5 + 0x29b4);
        iVar5 = iVar5 + 1;
        iVar4 = iVar4 + 1;
      } while (uVar6 < uVar3);
    }
  }
  FUN_001db8a8(0x7bfc40,0x6b07a0,0x29f4);
  uRam007c25d4 = uRam007bfc24;
  uRam007c25dc = uRam007bfc2c;
  uRam007c25ec = uRam007bfbdc;
  uVar6 = 0;
  uRam007c2630 = uRam007bfc20;
  if (uRam007bfc20 != 0) {
    iVar5 = 0x7bd230;
    iVar4 = 0x7bfc40;
    do {
      uVar6 = uVar6 + 1;
      *(undefined1 *)(iVar4 + 0x29b4) = *(undefined1 *)(iVar5 + 0x29b4);
      iVar5 = iVar5 + 1;
      iVar4 = iVar4 + 1;
    } while (uVar6 < uRam007c2630);
  }
  FUN_001dba60(0x7bd230,0,0x2a10);
  bRam007bfc40 = bRam007bfc40 & 0xf7;
  FUN_001db8a8(0x7bd230,0x7bfc40,0x29f4);
  return;
}

