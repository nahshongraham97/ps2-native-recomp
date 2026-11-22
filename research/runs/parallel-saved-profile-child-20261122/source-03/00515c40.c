
undefined4 FUN_00515c40(undefined8 param_1)

{
  undefined4 uVar1;
  long lVar2;
  int iVar3;
  int iVar4;
  uint uVar5;
  int iVar6;
  int iVar7;
  uint uVar8;
  
  uVar5 = 0;
  iVar3 = (int)param_1;
  iVar4 = 0x7bd230;
  do {
    if (*(uint *)(iVar4 + 0xac) == (*(uint *)(iVar3 + 0xc) | 0x10000000)) {
      iVar4 = uVar5 * 0xa28 + 0x7bd2dc;
      goto LAB_00515cc0;
    }
    uVar5 = uVar5 + 1;
    iVar4 = iVar4 + 0xa28;
  } while (uVar5 < 4);
  iVar4 = 0;
LAB_00515cc0:
  if (iVar4 == 0) {
    uVar1 = 0;
  }
  else {
    uVar5 = 0;
    iVar6 = 0;
    iVar7 = iVar4;
    do {
      uVar8 = 0;
      do {
        if ((*(uint *)(iVar7 + 0xa18) & 1 << (uVar8 & 0x1f)) != 0) {
          FUN_004a9a50(param_1,uVar8 + iVar6);
        }
        uVar8 = uVar8 + 1;
      } while (uVar8 < 0x20);
      uVar5 = uVar5 + 1;
      iVar7 = iVar7 + 4;
      iVar6 = iVar6 + 0x20;
    } while (uVar5 < 4);
    uVar5 = 0;
    iVar7 = iVar4;
    if (*(int *)(iVar4 + 0x470) != 0) {
      do {
        lVar2 = FUN_002163c0(*(undefined4 *)(iVar3 + 0xb24),*(undefined2 *)(iVar7 + 0x476));
        if (lVar2 != 0) {
          iRam0064f298 = iVar3;
          lVar2 = FUN_00443430(*(undefined4 *)(iVar7 + 0x47c),*(undefined4 *)(iVar7 + 0x480),
                               *(undefined4 *)(iVar7 + 0x484),*(undefined4 *)(iVar7 + 0x488),
                               *(undefined1 *)(iVar7 + 0x474),lVar2,*(undefined4 *)(iVar7 + 0x48c),
                               *(undefined4 *)(iVar7 + 0x498),*(undefined4 *)(iVar7 + 0x49c),
                               iVar7 + 0x4a0);
          if (lVar2 != 0) {
            if ((long)((ulong)*(byte *)(iVar7 + 0x478) << 0x3d) < 0) {
              FUN_00443ba0(lVar2);
            }
            iVar6 = (int)lVar2;
            *(byte *)(iVar6 + 0x34) = *(byte *)(iVar6 + 0x34) & 0xfe | *(byte *)(iVar7 + 0x478) & 1;
            *(byte *)(iVar6 + 0x34) =
                 *(byte *)(iVar6 + 0x34) & 0xfd |
                 (byte)((uint)(((ulong)*(byte *)(iVar7 + 0x478) << 0x3e) >> 0x3f) << 1);
            if ((long)((ulong)*(byte *)(iVar7 + 0x478) << 0x3c) < 0) {
              FUN_00443a90(*(undefined4 *)(iVar7 + 0x494),*(undefined4 *)(iVar7 + 0x490),lVar2);
            }
          }
          iRam0064f298 = 0;
        }
        uVar5 = uVar5 + 1;
        iVar7 = iVar7 + 0x40;
      } while (uVar5 < *(uint *)(iVar4 + 0x470));
    }
    uVar1 = 1;
  }
  return uVar1;
}

