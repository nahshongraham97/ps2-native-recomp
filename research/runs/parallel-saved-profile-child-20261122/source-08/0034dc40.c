
undefined4 FUN_0034dc40(undefined8 param_1,long param_2)

{
  bool bVar1;
  undefined4 uVar2;
  long lVar3;
  int iVar4;
  int iVar5;
  
  iVar5 = 0;
  bVar1 = false;
  iVar4 = 0;
  do {
    if (param_2 == 0) {
      FUN_001dba60(iRam0064f120 + iVar4,0,0x120);
    }
    else {
      lVar3 = FUN_001deb10(iRam0064f120 + iVar4,param_2);
      if (lVar3 == 0) {
        bVar1 = true;
      }
      if (bVar1) {
        FUN_001db8a8(iRam0064f120 + iVar4,iRam0064f120 + iVar4 + 0x120,0x120);
      }
    }
    iVar5 = iVar5 + 1;
    iVar4 = iVar4 + 0x120;
  } while (iVar5 < 5);
  if (bVar1) {
    FUN_001dba60(iRam0064f120 + 0x5a0,0,0x120);
    uVar2 = 1;
  }
  else {
    uVar2 = 0xffffffff;
  }
  return uVar2;
}

