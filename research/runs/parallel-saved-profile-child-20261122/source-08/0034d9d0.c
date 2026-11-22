
undefined4 FUN_0034d9d0(undefined8 param_1,long param_2)

{
  int iVar1;
  int iVar2;
  
  FUN_003d08b0();
  if (param_2 != -1) {
    iVar2 = 0;
    iVar1 = 0;
    do {
      FUN_003d0520(param_1,iRam0064f120 + (int)param_2 * 0x120 + iVar1 + 0x12);
      iVar2 = iVar2 + 1;
      iVar1 = iVar1 + 9;
    } while (iVar2 < 0x1e);
  }
  return 1;
}

