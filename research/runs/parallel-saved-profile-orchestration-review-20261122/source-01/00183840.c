
int FUN_00183840(long param_1,undefined4 param_2,undefined4 param_3)

{
  long lVar1;
  long lVar2;
  int iVar3;
  undefined4 uVar4;
  int iStack_4;
  
  uVar4 = 0;
  iStack_4 = 0;
  lVar1 = FUN_0012dcc0(0x3009,0x20,0x183610,0x14,&iStack_4);
  if (lVar1 != 0) {
    *(int *)(iStack_4 + 8) = (int)param_1;
    *(undefined4 *)(iStack_4 + 0xc) = param_2;
    *(undefined4 *)(iStack_4 + 0x10) = param_3;
  }
  if (param_1 == 1) {
    uGpffffa824 = 0;
    uGpffffa820 = 0xffffffff;
    uGpffffa81c = 0xffffffff;
    iGpffffa1a8 = 0;
  }
  else {
    if (param_1 != 0) {
      return 0;
    }
    uGpffffa830 = 0;
    uGpffffa82c = 0xffffffff;
    uGpffffa828 = 0xffffffff;
    iGpffffa1a4 = 0;
  }
  lVar2 = FUN_00281830(param_1);
  if (lVar1 == 0) {
    iVar3 = 0;
  }
  else {
    iVar3 = iGpffffa1a8;
    if (param_1 == 1) {
      while (iGpffffa1a8 = iVar3, iVar3 == 0) {
        uGpffff9e68 = 0x3f800000;
        (**(code **)(*piGpffff9e64 + 0x14))();
        iVar3 = iGpffffa1a8;
      }
      if (iVar3 == 2) {
        uVar4 = *(undefined4 *)(iGpffffa0d0 + 0x70);
      }
    }
    else {
      iVar3 = iGpffffa1a4;
      if (param_1 != 0) {
        return 0;
      }
      while (iGpffffa1a4 = iVar3, iVar3 == 0) {
        uGpffff9e68 = 0x3f800000;
        (**(code **)(*piGpffff9e64 + 0x14))();
        iVar3 = iGpffffa1a4;
      }
      if (iVar3 == 2) {
        uVar4 = *(undefined4 *)(iGpffffa0cc + 0x70);
      }
    }
    if (lVar2 != 0) {
      *(undefined4 *)lVar2 = uVar4;
    }
    FUN_002f09e0(0x1fe4,(int)param_1 + 1);
  }
  return iVar3;
}

