
void FUN_00241210(undefined4 *param_1)

{
  ulong uVar1;
  uint uVar2;
  undefined4 *puVar3;
  uint uVar4;
  int iVar5;
  int iVar6;
  undefined1 auStack_80 [32];
  undefined1 auStack_60 [16];
  undefined1 auStack_50 [16];
  undefined1 auStack_40 [16];
  undefined1 auStack_30 [16];
  undefined1 auStack_20 [16];
  undefined1 auStack_10 [16];
  
  iVar6 = 0;
  puVar3 = param_1;
  do {
    FUN_001dec58(*puVar3,0x5fefe0);
    iVar6 = iVar6 + 1;
    puVar3 = puVar3 + 1;
  } while (iVar6 < 3);
  iVar6 = 3;
  puVar3 = param_1 + 3;
  do {
    FUN_001dec58(*puVar3,0x5fefe8);
    iVar6 = iVar6 + 1;
    puVar3 = puVar3 + 1;
  } while (iVar6 < 8);
  FUN_001dec58(param_1[8],0x5feff8);
  if (iGpffff863c < 0) {
    iVar6 = 0;
  }
  else if (((iGpffff863c < 2) && (-1 < iGpffff8640)) && (iGpffff8640 < 8)) {
    iVar6 = iGpffff8640 * 0x3c30 + iGpffff863c * 0x1e218 + 0x6606d0;
  }
  else {
    iVar6 = 0;
  }
  if (iVar6 != 0) {
    uVar2 = *(uint *)(iVar6 + 0x1c);
    uVar4 = *(uint *)(iVar6 + 0x20);
    if (999999 < uVar2) {
      uVar2 = 999999;
    }
    if (999999 < uVar4) {
      uVar4 = 999999;
    }
    FUN_00242e10(auStack_10,uVar2);
    FUN_00242e10(auStack_20,uVar4);
    FUN_001de4b0(auStack_80,0x5fefd8,auStack_10,auStack_20);
    FUN_001dec58(*param_1,auStack_80);
    uVar2 = *(uint *)(iVar6 + 0x24);
    uVar4 = *(uint *)(iVar6 + 0x28);
    if (999999 < uVar2) {
      uVar2 = 999999;
    }
    if (999999 < uVar4) {
      uVar4 = 999999;
    }
    FUN_00242e10(auStack_30,uVar2);
    FUN_00242e10(auStack_40,uVar4);
    FUN_001de4b0(auStack_80,0x5fefd8,auStack_30,auStack_40);
    FUN_001dec58(param_1[1],auStack_80);
    uVar2 = *(uint *)(iVar6 + 0x8f8);
    uVar4 = *(uint *)(iVar6 + 0x8fc);
    if (999999 < uVar2) {
      uVar2 = 999999;
    }
    if (999999 < uVar4) {
      uVar4 = 999999;
    }
    FUN_00242e10(auStack_50,uVar2);
    FUN_00242e10(auStack_60,uVar4);
    FUN_001de4b0(auStack_80,0x5fefd8,auStack_50,auStack_60);
    FUN_001dec58(param_1[2],auStack_80);
    iVar5 = 3;
    puVar3 = param_1 + 3;
    do {
      uVar1 = *(ulong *)(iVar6 + (iVar5 + -3) * 8 + 0x900);
      if (uVar1 >> 0x20 == 0) {
        FUN_001dec58(*puVar3,0x5fefe8);
      }
      else {
        FUN_001de4b0(*puVar3,0x5fefc8,(uint)(uVar1 >> 0x10) & 0xff,(uint)(uVar1 >> 8) & 0xff,
                     (uint)uVar1 & 0xff);
      }
      iVar5 = iVar5 + 1;
      puVar3 = puVar3 + 1;
    } while (iVar5 < 8);
    FUN_00242e10(auStack_80,*(undefined4 *)(iVar6 + 0x40));
    FUN_001dec58(param_1[8],auStack_80);
  }
  return;
}

