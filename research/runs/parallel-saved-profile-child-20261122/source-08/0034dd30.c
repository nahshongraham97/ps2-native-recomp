
undefined8 FUN_0034dd30(undefined8 param_1,long param_2,long param_3)

{
  long lVar1;
  int iVar2;
  int iVar3;
  
  if (param_3 != 0) {
    *(int *)param_3 = -1;
  }
  if ((param_2 != 0) && (iRam0064f120 != 0)) {
    iVar3 = 0;
    iVar2 = 0;
    do {
      lVar1 = FUN_001deb10(iRam0064f120 + iVar2,param_2);
      if (lVar1 == 0) {
        if (param_3 != 0) {
          *(int *)param_3 = iVar3;
          return 1;
        }
        return 1;
      }
      iVar3 = iVar3 + 1;
      iVar2 = iVar2 + 0x120;
    } while (iVar3 < 6);
  }
  return 0;
}

