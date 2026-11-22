
void FUN_00511350(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
                 undefined4 param_5,undefined4 param_6,undefined4 param_7,long param_8)

{
  byte bVar1;
  ulong uVar2;
  int iVar3;
  int iVar4;
  
  iVar4 = *(int *)(iGpffffb160 + 0x2c);
  FUN_00440660();
  FUN_00440520();
  bRam007bfc40 = bRam007bfc40 & 0xf7;
  uRam007bfcb8 = 0;
  iRam007bfc88 = *(int *)(iGpffffb160 + 0x158);
  iRam007bfc84 = (int)param_8;
  if (iRam007bfc88 == 4) {
    iRam007bfc84 = 9;
LAB_00511470:
    iVar3 = *(int *)(iGpffffb160 + 0x100);
  }
  else {
    if (iRam007bfc88 == 3) {
      iRam007bfc84 = iRam007bfc84 + 2;
      goto LAB_00511470;
    }
    if (iRam007bfc88 == 1) {
      if (2 < param_8) {
        iRam007bfc84 = iRam007bfc84 + -2;
      }
      goto LAB_00511470;
    }
    if (iRam007bfc88 == 0) {
      iRam007bfc84 = 1;
      goto LAB_00511470;
    }
    iVar3 = *(int *)(iGpffffb160 + 0x100);
  }
  if (iVar3 == 1) {
    if (((ulong)*(byte *)(iGpffffb160 + 0xc) & 7) >> 2 == 0) {
      iVar4 = *(int *)(iGpffffb160 + 0x30);
      uVar2 = 0;
      goto LAB_005114bc;
    }
    bVar1 = *(byte *)(iGpffffb160 + 0xc);
  }
  else {
    bVar1 = *(byte *)(iGpffffb160 + 0xc);
  }
  uVar2 = ((ulong)bVar1 & 7) >> 2;
  if (uVar2 != 0) {
    iVar4 = *(int *)(iGpffffb160 + 0x30);
  }
LAB_005114bc:
  if (uVar2 != 0) {
    uRam007bfca4 = uRam007bfc3c;
    uRam007bfca8 = uRam007bfc8c;
    uRam007bfcac = uRam007bfc94;
  }
  uRam007bfc98 = 0x3d;
  uRam007bfc80 = *(undefined4 *)(iGpffffb160 + 0x104);
  iVar4 = *(int *)(iVar4 + 0xc);
  ppuRam007bfcc4 = (undefined **)(&PTR_PTR_005ed210)[iVar4 * 3];
  if (iVar4 == 0x2d) {
    ppuRam007bfcc4 = &PTR_PTR_0060cfe0;
  }
  else if (iVar4 == 0x2f) {
    ppuRam007bfcc4 = &PTR_PTR_00611830;
  }
  uRam007bfc3c = param_5;
  uRam007bfc8c = param_3;
  uRam007bfc94 = param_4;
  uRam007bfc9c = param_2;
  uRam007bfca0 = param_1;
  uRam007bfcb0 = param_6;
  uRam007bfcb4 = param_7;
  uRam007bfcbc = param_2;
  uRam007bfcc0 = param_7;
  FUN_0012dda0(0xd018,0x20,0x5117c0,0,0);
  return;
}

