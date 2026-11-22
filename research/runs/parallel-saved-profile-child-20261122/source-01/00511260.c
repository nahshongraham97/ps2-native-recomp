
void FUN_00511260(long param_1,undefined8 param_2)

{
  int iVar1;
  
  iVar1 = (int)param_2;
  if (param_1 == 0) {
    *(undefined4 *)(iVar1 + 0x2c) = uRam007bfc98;
    *(byte *)(iVar1 + 0x14) = *(byte *)(iVar1 + 0x14) & 0xfe;
    if (iRam007bfc80 == 1) {
      *(byte *)(iVar1 + 0x14) = *(byte *)(iVar1 + 0x14) & 0xfe | 1;
    }
    FUN_001319e0(param_2,2);
  }
  else {
    if (iRam007bfcb8 == 0) {
      *(undefined4 *)(iVar1 + 0x2c) = uRam007bfc9c;
    }
    else {
      *(undefined4 *)(iVar1 + 0x2c) = uRam007bfcb4;
      uRam007bfcb4 = 0x40;
    }
    *(byte *)(iVar1 + 0x14) = *(byte *)(iVar1 + 0x14) & 0xfe;
    FUN_001319e0(param_2,0);
  }
  return;
}

