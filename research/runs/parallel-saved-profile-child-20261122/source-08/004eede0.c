
byte * FUN_004eede0(undefined8 param_1)

{
  byte *pbVar1;
  undefined1 auStack_4 [4];
  
  pbVar1 = (byte *)FUN_00245bd0(param_1,auStack_4);
  if ((*pbVar1 & 1) == 0) {
    pbVar1 = &DAT_00646e88;
  }
  else {
    pbVar1 = pbVar1 + 0x2e8;
  }
  return pbVar1;
}

