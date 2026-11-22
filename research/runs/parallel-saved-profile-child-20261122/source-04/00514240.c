
void FUN_00514240(long param_1)

{
  undefined1 uVar1;
  undefined1 uVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  int iVar5;
  uint uVar6;
  int iVar7;
  uint uVar8;
  
  FUN_001dba60(0x7bd230,0,0x2a10);
  FUN_00515450();
  bRam007bfc40 = bRam007bfc40 & 0xfe;
  if (-1 < param_1) {
    uVar8 = 0;
    iVar7 = 0x7bfc40;
    iVar5 = 0x7bd230;
    do {
      uVar8 = uVar8 + 5;
      uVar3 = *(undefined4 *)(iVar7 + 0x2964);
      uVar4 = *(undefined4 *)(iVar7 + 0x2968);
      *(undefined4 *)(iVar5 + 0x2960) = *(undefined4 *)(iVar7 + 0x2960);
      *(undefined4 *)(iVar5 + 0x2964) = uVar3;
      *(undefined4 *)(iVar5 + 0x2968) = uVar4;
      uVar3 = *(undefined4 *)(iVar7 + 0x2970);
      *(undefined4 *)(iVar5 + 0x296c) = *(undefined4 *)(iVar7 + 0x296c);
      *(undefined4 *)(iVar5 + 0x2970) = uVar3;
      iVar7 = iVar7 + 0x14;
      iVar5 = iVar5 + 0x14;
    } while (uVar8 < 10);
    uVar8 = 0;
    uRam007bfc20 = uRam007c2630;
    if (uRam007c2630 != 0) {
      uVar6 = uRam007c2630 - 8;
      if (8 < uRam007c2630) {
        iVar7 = 0x7bfc40;
        iVar5 = 0x7bd230;
        do {
          uVar8 = uVar8 + 8;
          uVar1 = *(undefined1 *)(iVar7 + 0x29b5);
          uVar2 = *(undefined1 *)(iVar7 + 0x29b6);
          *(undefined1 *)(iVar5 + 0x29b4) = *(undefined1 *)(iVar7 + 0x29b4);
          *(undefined1 *)(iVar5 + 0x29b5) = uVar1;
          *(undefined1 *)(iVar5 + 0x29b6) = uVar2;
          uVar1 = *(undefined1 *)(iVar7 + 0x29b8);
          uVar2 = *(undefined1 *)(iVar7 + 0x29b9);
          *(undefined1 *)(iVar5 + 0x29b7) = *(undefined1 *)(iVar7 + 0x29b7);
          *(undefined1 *)(iVar5 + 0x29b8) = uVar1;
          *(undefined1 *)(iVar5 + 0x29b9) = uVar2;
          uVar1 = *(undefined1 *)(iVar7 + 0x29bb);
          *(undefined1 *)(iVar5 + 0x29ba) = *(undefined1 *)(iVar7 + 0x29ba);
          *(undefined1 *)(iVar5 + 0x29bb) = uVar1;
          iVar7 = iVar7 + 8;
          iVar5 = iVar5 + 8;
        } while (uVar8 < uVar6);
      }
      uVar6 = uRam007c2630;
      if (uVar8 < uRam007c2630) {
        iVar7 = uVar8 + 0x7bfc40;
        iVar5 = uVar8 + 0x7bd230;
        do {
          uVar8 = uVar8 + 1;
          *(undefined1 *)(iVar5 + 0x29b4) = *(undefined1 *)(iVar7 + 0x29b4);
          iVar7 = iVar7 + 1;
          iVar5 = iVar5 + 1;
        } while (uVar8 < uVar6);
      }
    }
    uRam007bfc44 = 0;
    iRam007bd244 = (int)param_1;
    uRam007bfc78 = (&DAT_005f2550)[iRam007bd244];
    iRam007bfc54 = iRam007bd244;
  }
  return;
}

