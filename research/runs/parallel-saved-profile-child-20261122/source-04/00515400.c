
bool FUN_00515400(uint param_1)

{
  bool bVar1;
  
  if (param_1 >> 5 < 6) {
    bVar1 = (*(uint *)((param_1 >> 5) * 4 + 0x7bfb7c) & 1 << (param_1 & 0x1f)) != 0;
  }
  else {
    bVar1 = false;
  }
  return bVar1;
}

