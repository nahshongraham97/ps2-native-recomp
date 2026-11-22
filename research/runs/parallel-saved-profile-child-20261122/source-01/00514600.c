
undefined4 FUN_00514600(void)

{
  if ((bRam007bfc40 & 1) == 1) {
    uRam007c25d8 = uRam007c25d4;
    uRam007c25e0 = uRam007c25dc;
    uRam007bfc78 = uRam007bfc74;
    uRam007bfc44 = 5;
  }
  else {
    uRam007bfc44 = 0;
  }
  *(int *)(iRam0064e1bc + 0x3c) = *(int *)(iRam0064e1bc + 0x3c) + iRam007c25ec;
  *(int *)(iRam0064e1bc + 0x38) = *(int *)(iRam0064e1bc + 0x38) + iRam007c25ec;
  iRam007c25ec = 0;
  bRam007bfc40 = bRam007bfc40 & 0xf7;
  FUN_001db8a8(0x6b07a0,0x7bfc40,0x29f4);
  FUN_001834b0(0,1);
  return 0;
}

