
undefined4 FUN_005117c0(void)

{
  int iVar1;
  uint uVar2;
  
  if ((long)((ulong)*(byte *)(iGpffffb160 + 0xc) << 0x3d) < 0) {
    uRam007bfc44 = 6;
  }
  else {
    uRam007bfc44 = 7;
  }
  uVar2 = 0;
  iVar1 = 0;
  do {
    if ((long)((ulong)*(byte *)(*(int *)(iGpffffb160 + iVar1 + 0x2c) + 8) << 0x3d) < 0) {
      FUN_00511f00();
    }
    uVar2 = uVar2 + 1;
    iVar1 = iVar1 + 4;
  } while (uVar2 < 2);
  FUN_005163f0(0x7bfc24);
  FUN_004517b0();
  uGpffff9db4 = 9;
  FUN_001225d0(2,0x16a4c0);
  return 0xbf800000;
}

