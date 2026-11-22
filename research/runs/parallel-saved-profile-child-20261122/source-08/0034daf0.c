
undefined4 FUN_0034daf0(undefined8 param_1)

{
  long lVar1;
  int iVar2;
  int iVar3;
  
  iVar3 = 0;
  iVar2 = 0;
  do {
    lVar1 = FUN_001dee00(iRam0064f120 + iVar2);
    if (lVar1 != 0) {
      FUN_003d6cd0(param_1,iRam0064f120 + iVar2,iRam0064f120 + iVar2 + 9);
    }
    iVar3 = iVar3 + 1;
    iVar2 = iVar2 + 0x120;
  } while (iVar3 < 6);
  return 1;
}

