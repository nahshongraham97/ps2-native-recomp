
undefined8 FUN_00183330(long param_1,undefined8 param_2)

{
  int iVar1;
  long lVar2;
  undefined8 uVar3;
  int iVar4;
  int iVar5;
  
  iVar4 = iGpffffa824;
  if ((param_1 == 1) || (iVar4 = iGpffffa830, param_1 == 0)) {
    if (iVar4 == 1) {
      lVar2 = FUN_00243a20(param_1);
      if (lVar2 == 0) {
        uGpffffa918 = 0;
        uVar3 = 0;
      }
      else {
        iVar4 = iGpffffa81c;
        iVar1 = iGpffffa820;
        if ((param_1 == 1) || (iVar4 = iGpffffa828, iVar1 = iGpffffa82c, param_1 == 0)) {
          if ((iVar1 < 0) || (1 < iVar1)) {
            uGpffffa918 = 0;
            uVar3 = 0;
          }
          else {
            iVar5 = iVar1 * 0x1e218;
            FUN_00244ce0(param_1,iVar5 + iVar4 * 0x3c30 + 0x6606d0);
            uVar3 = FUN_00263e40(0x10,1);
            uVar3 = FUN_00171490(iVar1,param_2,uVar3,iVar5 + 0x660678,0,iVar5 + 0x660644,
                                 iVar5 + 0x660648);
          }
        }
        else {
          uVar3 = 0;
        }
      }
    }
    else {
      uGpffffa918 = 0;
      uVar3 = 0;
    }
  }
  else {
    uVar3 = 0;
  }
  return uVar3;
}

