
undefined4 FUN_004ef1c0(undefined8 param_1)

{
  undefined4 uVar1;
  long lVar2;
  undefined1 auStack_4 [4];
  
  if ((bRam00660084 & 1) == 0) {
    uVar1 = 0;
  }
  else {
    lVar2 = FUN_00245bd0(param_1,auStack_4);
    if (lVar2 == 0) {
      uVar1 = 0;
    }
    else {
      uVar1 = 0;
      if ((*(byte *)lVar2 & 1) != 0) {
        uVar1 = 1;
      }
    }
  }
  return uVar1;
}

