
undefined4 FUN_004eee60(undefined8 param_1)

{
  undefined4 uVar1;
  byte *pbVar2;
  long lVar3;
  undefined1 auStack_4 [4];
  
  if ((long)((ulong)bRam00660084 << 0x3d) < 0) {
    uVar1 = 1;
  }
  else {
    pbVar2 = (byte *)FUN_00245bd0(param_1,auStack_4);
    if ((*pbVar2 & 1) == 0) {
      uVar1 = 1;
    }
    else if (((bRam00660084 & 1) == 0) || (lVar3 = FUN_00340bf0(), lVar3 != 0)) {
      lVar3 = FUN_0012ed10(0x9012);
      if (lVar3 == 0) {
        uVar1 = 0;
      }
      else {
        uVar1 = 1;
      }
    }
    else {
      uVar1 = 1;
    }
  }
  return uVar1;
}

