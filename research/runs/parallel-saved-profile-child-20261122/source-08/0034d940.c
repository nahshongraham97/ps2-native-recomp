
undefined4 FUN_0034d940(undefined8 param_1,int param_2)

{
  undefined8 uVar1;
  int iVar2;
  int iVar3;
  
  if (param_2 != -1) {
    iVar3 = 0;
    iVar2 = 0;
    do {
      uVar1 = FUN_003d0230(param_1,iVar3);
      FUN_001df378(iRam0064f120 + param_2 * 0x120 + iVar2 + 0x12,uVar1,9);
      iVar3 = iVar3 + 1;
      iVar2 = iVar2 + 9;
    } while (iVar3 < 0x1e);
  }
  return 1;
}

