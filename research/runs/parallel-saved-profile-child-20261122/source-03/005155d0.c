
undefined4 FUN_005155d0(int param_1)

{
  byte bVar1;
  int iVar2;
  undefined4 uVar3;
  long lVar4;
  undefined8 uVar5;
  int iVar6;
  uint uVar7;
  int iVar8;
  int *piVar9;
  
  uVar7 = 0;
  iVar6 = 0x7bd230;
  do {
    if (*(uint *)(iVar6 + 0xac) == (*(uint *)(param_1 + 0xc) | 0x10000000)) {
      iVar6 = uVar7 * 0xa28 + 0x7bd2dc;
      goto LAB_00515650;
    }
    uVar7 = uVar7 + 1;
    iVar6 = iVar6 + 0xa28;
  } while (uVar7 < 4);
  iVar6 = 0;
LAB_00515650:
  if (iVar6 == 0) {
    uVar3 = 0;
  }
  else {
    uVar7 = 0;
    if (*(int *)(iVar6 + 0xa0c) != 0) {
      iVar8 = 0;
      do {
        iVar2 = FUN_00215df0(0x647af0,0,*(undefined4 *)(param_1 + 0xb24));
        iRam0064f298 = param_1;
        if ((*(int *)(iVar2 + iVar8 + 0x28) == 0xf4) || (lVar4 = FUN_004471a0(), lVar4 == 0)) {
          bVar1 = *(byte *)(iVar6 + uVar7 + 0xa10);
          piVar9 = (int *)(iVar2 + (uint)bVar1 * 0x44);
          iVar2 = *piVar9;
          if (iVar2 == 0) {
            uVar5 = FUN_00437720(*(undefined4 *)(param_1 + 0x10),bVar1);
            FUN_00443420(piVar9[1],0x3f266666,piVar9[3],0,uVar5,0x5ed9b0,0,bVar1);
          }
          else if (iVar2 == 2) {
            uVar5 = FUN_00437720(*(undefined4 *)(param_1 + 0x10),bVar1);
            FUN_00443420(piVar9[1],piVar9[2],piVar9[3],0,uVar5,0x5ed9f0,0,bVar1);
          }
          else if (iVar2 == 1) {
            uVar5 = FUN_00437720(*(undefined4 *)(param_1 + 0x10),bVar1);
            FUN_00443420(piVar9[1],0x3f266666,piVar9[3],0,uVar5,0x5eda30,0,bVar1);
          }
        }
        iRam0064f298 = 0;
        uVar7 = uVar7 + 1;
        iVar8 = iVar8 + 0x44;
      } while (uVar7 < *(uint *)(iVar6 + 0xa0c));
    }
    uVar3 = 1;
  }
  return uVar3;
}

