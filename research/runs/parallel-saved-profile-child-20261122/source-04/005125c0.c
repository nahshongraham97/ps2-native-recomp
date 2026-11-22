
void FUN_005125c0(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
                 undefined4 param_5,undefined4 param_6,undefined4 param_7)

{
  long lVar1;
  int iVar2;
  
  lVar1 = FUN_00182d90(0xc026);
  if (iGpffffb160 != 0) {
    FUN_0012bf10(0x1041baa);
    FUN_00511f00(*(undefined4 *)(iGpffffb160 + 0x2c));
    FUN_00511f00(*(undefined4 *)(iGpffffb160 + 0x30));
    iVar2 = iRam007c25ec;
    FUN_001db8a8(0x7bfc40,0x7bd230,0x29f4);
    iRam007c25ec = iRam007c25ec + iVar2;
    uRam007bfbdc = 0;
    uRam007bfc7c = *(undefined4 *)(iGpffffb160 + 0x104);
    uRam007c25d4 = *(undefined4 *)(iGpffffb160 + 100);
    uRam007c25d8 = *(undefined4 *)(iGpffffb160 + 0x68);
    bRam007bfc40 = bRam007bfc40 & 0xfd;
    uRam007c25dc = *(undefined4 *)(iGpffffb160 + 0x9c);
    uRam007c25e0 = *(undefined4 *)(iGpffffb160 + 0xa0);
    uRam007c25e4 = *(undefined4 *)(iGpffffb160 + 0x928);
    uRam007c25e8 = *(undefined4 *)(iGpffffb160 + 0x924);
    if (lVar1 != 0) {
      bRam007bfc40 = bRam007bfc40 | 2;
      iVar2 = (int)lVar1;
      uRam007bfc48 = *(undefined4 *)(iVar2 + 0xc0);
      uRam007bfc4c = *(undefined4 *)(iVar2 + 0xc4);
      uRam007bfc50 = *(undefined4 *)(iVar2 + 200);
    }
    bRam007bfc40 = bRam007bfc40 & 0xfe | 1;
    uRam007bfc74 = *(undefined4 *)
                    (*(int *)(*(int *)(iGpffffb160 + 0x100) * 4 + iGpffffb160 + 0x2c) + 0xc);
    uRam007bfc58 = param_1;
    uRam007bfc5c = param_2;
    uRam007bfc60 = param_3;
    uRam007bfc64 = param_4;
    uRam007bfc68 = param_5;
    uRam007bfc6c = param_6;
    uRam007bfc70 = param_7;
    FUN_005127f0();
  }
  return;
}

