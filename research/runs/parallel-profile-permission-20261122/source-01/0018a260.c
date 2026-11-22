
undefined4 FUN_0018a260(long param_1)

{
  int iVar1;
  undefined4 uVar2;
  long lVar3;
  int *piVar4;
  undefined8 uVar5;
  
  lVar3 = FUN_002f18f0();
  if (lVar3 == 0) {
    lVar3 = 0;
  }
  if (lVar3 == 0) {
    uVar2 = 0;
  }
  else {
    piVar4 = (int *)((uint)(param_1 == 0) * 0x48 + 0x660134);
    if (*piVar4 == 2) {
      uVar2 = 0;
    }
    else {
      iVar1 = (int)lVar3;
      if (param_1 == 0) {
        if ((bRam00660140 & 1) == 0) {
          *(undefined4 *)(iVar1 + 0x2c) = 1;
        }
        else {
          *(undefined4 *)(iVar1 + 0x2c) = 2;
        }
        *(undefined4 *)(iVar1 + 0x24) = 0;
      }
      else {
        if ((bRam00660188 & 1) == 0) {
          *(undefined4 *)(iVar1 + 0x58) = 1;
        }
        else {
          *(undefined4 *)(iVar1 + 0x58) = 2;
        }
        *(undefined4 *)(iVar1 + 0x50) = 0;
      }
      *(int *)(iVar1 + 0x60) = *(int *)(iVar1 + 0x60) + -1;
      *(undefined4 *)((int)param_1 * 0x48 + 0x660134) = 1;
      if (iGpffff9db4 == 4) {
        *piVar4 = 0;
      }
      if (param_1 == 0) {
        uVar5 = 0;
      }
      else {
        uVar5 = 9;
      }
      FUN_0018b0d0(param_1,uVar5);
      uVar2 = 1;
    }
  }
  return uVar2;
}

