
undefined4 FUN_00515e60(undefined8 param_1)

{
  byte bVar1;
  undefined4 uVar2;
  long lVar3;
  int iVar4;
  uint uVar5;
  int iVar6;
  
  uVar5 = 0;
  iVar4 = 0x7bd230;
  do {
    if (*(uint *)(iVar4 + 0xac) == (*(uint *)((int)param_1 + 0xc) | 0x10000000)) {
      iVar4 = uVar5 * 0xa28 + 0x7bd2dc;
      goto LAB_00515edc;
    }
    uVar5 = uVar5 + 1;
    iVar4 = iVar4 + 0xa28;
  } while (uVar5 < 4);
  iVar4 = 0;
LAB_00515edc:
  if (iVar4 == 0) {
    uVar2 = 0;
  }
  else {
    uVar5 = 0;
    iVar6 = iVar4;
    if (*(int *)(iVar4 + 0x3a4) != 0) {
      do {
        lVar3 = FUN_002163c0(*(undefined4 *)((int)param_1 + 0xb24),*(undefined2 *)(iVar6 + 0x3a8));
        if (lVar3 != 0) {
          bVar1 = *(byte *)(iVar6 + 0x3aa);
          *(uint *)((int)lVar3 + 0x24) = (uint)bVar1;
          *(uint *)((int)lVar3 + 0x24) = bVar1 & 0xfffffff7;
          FUN_0045f330(param_1,lVar3);
        }
        uVar5 = uVar5 + 1;
        iVar6 = iVar6 + 4;
      } while (uVar5 < *(uint *)(iVar4 + 0x3a4));
    }
    uVar2 = 1;
  }
  return uVar2;
}

