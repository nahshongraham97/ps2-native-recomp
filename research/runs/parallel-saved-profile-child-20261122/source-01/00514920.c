
void FUN_00514920(void)

{
  if (iRam0064e124 == 1) {
    FUN_00133020(uRam00660168,0x514a50);
  }
  else {
    FUN_00133020(uRam006601b0,0x514a50);
  }
  FUN_00514a90();
  FUN_001db8a8(0x7bfc40,0x6b07a0,0x29f4);
  bRam007bfc40 = bRam007bfc40 & 0xef;
  FUN_00515240(0xe,1);
  FUN_00515240(0xf,1);
  *(int *)(iRam0064e1bc + 0x3c) = *(int *)(iRam0064e1bc + 0x3c) + iRam007c25ec;
  *(int *)(iRam0064e1bc + 0x38) = *(int *)(iRam0064e1bc + 0x38) + iRam007c25ec;
  iRam007c25ec = 0;
  bRam007bfc40 = bRam007bfc40 & 0xf7;
  FUN_001db8a8(0x6b07a0,0x7bfc40,0x29f4);
  return;
}

