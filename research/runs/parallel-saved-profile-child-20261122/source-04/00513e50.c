
void FUN_00513e50(void)

{
  int iVar1;
  int iVar2;
  int iVar3;
  
  iVar1 = FUN_001818d0(0);
  iVar2 = *(int *)(iGpffffb160 + 0x138);
  if (iVar2 == 0) {
    iVar2 = 0;
  }
  else if (*(int *)(iVar2 + 4) != *(int *)(iGpffffb160 + 0x13c)) {
    iVar2 = 0;
  }
  iVar3 = *(int *)(iGpffffb160 + 0x2c);
  if ((*(uint *)(&DAT_005ed218 + *(int *)(iGpffffb160 + 0x34) * 0xc) & 1) != 0) {
    iVar3 = *(int *)(iGpffffb160 + 0x30);
  }
  *(byte *)(iVar2 + 0xe) = *(byte *)(iVar2 + 0xe) & 0xbf;
  FUN_00132ec0(iVar2,0x7bfc58,0x7bfc64);
  uRam007b9898 = uRam007bfc58;
  uRam007b989c = uRam007bfc5c;
  uRam007b98a0 = uRam007bfc60;
  FUN_0047a5f0(0,0);
  FUN_0047c720(*(int *)(iVar3 + 0xb14) + 0x50);
  *(undefined4 *)(iVar1 + 0x30) = *(undefined4 *)(*(int *)(iVar3 + 0xb14) + 0x5c);
  *(undefined4 *)(iVar1 + 0x34) = *(undefined4 *)(*(int *)(iVar3 + 0xb14) + 0x60);
  *(undefined4 *)(iVar1 + 0x1c) = uRam007bfc70;
  *(undefined4 *)(iVar1 + 0x18) = *(undefined4 *)(iVar1 + 0x30);
  *(undefined4 *)(iGpffffb160 + 0x850) = 0x78;
  FUN_00478b80();
  uGpffff9e68 = 0x3f800000;
  (**(code **)(*piGpffff9e64 + 0x14))();
  *(undefined4 *)(iGpffffb160 + 100) = uRam007c25d4;
  *(undefined4 *)(iGpffffb160 + 0x68) = uRam007c25d8;
  *(undefined4 *)(iGpffffb160 + 0x9c) = uRam007c25dc;
  *(undefined4 *)(iGpffffb160 + 0xa0) = uRam007c25e0;
  *(undefined4 *)(iGpffffb160 + 0x928) = uRam007c25e4;
  *(undefined4 *)(iGpffffb160 + 0x924) = uRam007c25e8;
  return;
}

