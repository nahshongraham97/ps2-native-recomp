
bool FUN_004ef180(undefined8 param_1)

{
  bool bVar1;
  long lVar2;
  undefined1 auStack_4 [4];
  
  lVar2 = FUN_00245bd0(param_1,auStack_4);
  if (lVar2 == 0) {
    bVar1 = false;
  }
  else {
    bVar1 = *(int *)((int)lVar2 + 0x1f8) == 1;
  }
  return bVar1;
}

