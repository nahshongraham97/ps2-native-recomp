
undefined4 FUN_00515310(uint param_1)

{
  undefined4 uVar1;
  
  uVar1 = 1;
  if (param_1 >> 5 < 0xb) {
    if ((*(uint *)((param_1 >> 5) * 4 + 0x7bfb90) & 1 << (param_1 & 0x1f)) == 0) {
      if (param_1 - 0xd < 2) {
        if ((uRam00660500 & 1 << (param_1 & 0x1f)) == 0) {
          uVar1 = 0;
        }
      }
      else {
        uVar1 = 0;
      }
    }
  }
  else {
    uVar1 = 0;
  }
  return uVar1;
}

