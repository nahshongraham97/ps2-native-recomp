
void FUN_0018b0d0(int param_1,undefined4 param_2)

{
  long lVar1;
  int iVar2;
  undefined4 *puVar3;
  int iVar4;
  
  if (param_1 == iRam00660130) {
    iVar4 = 0x66012c;
  }
  else {
    iVar4 = 0x660174;
  }
  lVar1 = FUN_002f18f0();
  if (lVar1 == 0) {
    lVar1 = 0;
  }
  iVar2 = (int)lVar1;
  if (param_1 == 0) {
    puVar3 = (undefined4 *)(iVar2 + 0x10);
  }
  else {
    puVar3 = (undefined4 *)(iVar2 + 0x3c);
  }
  if (lVar1 != 0) {
    if (((bRam00660084 & 1) == 0) || (param_1 != iGpffff86d4)) {
      if (*(int *)(param_1 * 0x48 + 0x660134) == 1) {
        if (param_1 == 0) {
          iVar2 = *(int *)(iVar2 + 0x10);
        }
        else {
          iVar2 = *(int *)(iVar2 + 0x3c);
        }
        lVar1 = FUN_001892d0((&DAT_005a2bc0)[iVar2 * 4],0,param_1);
        if (lVar1 != 0) {
          FUN_00189690(param_1 * 0x48 + 0x66012c);
          *puVar3 = param_2;
        }
        FUN_00188720(param_1);
        iVar2 = *(int *)(iVar4 + 8);
      }
      else {
        iVar2 = *(int *)(iVar4 + 8);
      }
      if (iVar2 == 2) {
        FUN_001319e0(iVar4,1);
        *puVar3 = param_2;
        FUN_00188720(param_1);
      }
    }
    else if (*(int *)(iVar4 + 8) != 0) {
      FUN_001319e0(iVar4,1);
      FUN_00188720(param_1);
    }
  }
  return;
}

