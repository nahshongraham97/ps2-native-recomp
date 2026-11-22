
undefined4 FUN_0034ddf0(undefined8 param_1,undefined8 param_2,undefined8 param_3,long param_4)

{
  long lVar1;
  int iVar2;
  int iVar3;
  
  if (param_4 != 0) {
    *(int *)param_4 = -1;
  }
  iVar3 = 0;
  iVar2 = 0;
  do {
    lVar1 = FUN_001dee00(iRam0064f120 + iVar2);
    if (lVar1 == 0) {
      FUN_001dec58(iRam0064f120 + iVar3 * 0x120,param_2);
      FUN_001dec58(iRam0064f120 + iVar3 * 0x120 + 9,param_3);
      if (param_4 == 0) {
        return 1;
      }
      *(int *)param_4 = iVar3;
      return 1;
    }
    iVar3 = iVar3 + 1;
    iVar2 = iVar2 + 0x120;
  } while (iVar3 < 6);
  return 0xffffffff;
}

