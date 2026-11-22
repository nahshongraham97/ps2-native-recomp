
void FUN_005103a0(void)

{
  uint uVar1;
  int iVar2;
  long lVar3;
  int iVar4;
  int iVar5;
  uint uVar6;
  int iVar7;
  int iVar8;
  int iVar9;
  int iVar10;
  undefined1 auStack_30 [48];
  
  lVar3 = FUN_0012ed10(0xd01c);
  if (((lVar3 != 0) && (lVar3 = FUN_0012e930(), lVar3 != 0)) &&
     (iVar2 = (int)lVar3, *(int *)(iVar2 + 0xc) != 0)) {
    lVar3 = FUN_00215df0(0x647830,0,*(undefined4 *)(*(int *)(iRam0064f250 + 0x30) + 0xb24));
    if (lVar3 == 0) {
      iVar7 = 0;
    }
    else {
      uVar1 = *(uint *)(*(int *)(iRam0064f250 + 0x9ac) * 4 + 0x7bfccc);
      if (uVar1 != uRam0064f378) {
        iVar7 = 0;
        iRam0064f37c = 0;
        uVar6 = 0;
        uRam0064f378 = uVar1;
        if (uVar1 != 0) {
          if (8 < uVar1) {
            iVar5 = 0;
            do {
              iVar4 = *(int *)((int)lVar3 + 8) + iVar5;
              uVar6 = uVar6 + 8;
              iVar5 = iVar5 + 0x2a0;
              iVar7 = iVar7 + *(int *)(iVar4 + 0x50) + *(int *)(iVar4 + 0xa4) +
                      *(int *)(iVar4 + 0xf8) + *(int *)(iVar4 + 0x14c) + *(int *)(iVar4 + 0x1a0) +
                      *(int *)(iVar4 + 500) + *(int *)(iVar4 + 0x248) + *(int *)(iVar4 + 0x29c);
              iRam0064f37c = iVar7;
            } while (uVar6 < uVar1 - 8);
          }
          if (uVar6 < uVar1) {
            iVar7 = uVar6 * 0x54;
            do {
              iVar5 = *(int *)((int)lVar3 + 8) + iVar7;
              uVar6 = uVar6 + 1;
              iVar7 = iVar7 + 0x54;
              iRam0064f37c = iRam0064f37c + *(int *)(iVar5 + 0x50);
            } while (uVar6 < uVar1);
          }
        }
      }
      iVar7 = iRam0064f37c + *(int *)(iVar2 + 0x10);
    }
    FUN_001de4b0(auStack_30,0x647a10,iVar7,*(undefined4 *)(iVar2 + 0x14));
    iVar2 = *(int *)(iRam0064f250 + 0x9fc);
    if (iVar2 == 0) {
      iVar2 = 0;
    }
    else if (*(int *)(iVar2 + 4) != *(int *)(iRam0064f250 + 0xa00)) {
      iVar2 = 0;
    }
    if (iVar2 == 0) {
      iVar2 = FUN_0016f800(0x831e,6,auStack_30,(int)((float)iRam0064def8 * 0.15),
                           iRam0064def4 + -0x7a,0x21);
      if (iVar2 != 0) {
        if (iVar2 == 0) {
          *(undefined4 *)(iRam0064f250 + 0x9fc) = 0;
          *(undefined4 *)(iRam0064f250 + 0xa00) = 0;
        }
        else {
          *(int *)(iRam0064f250 + 0x9fc) = iVar2;
          *(undefined4 *)(iRam0064f250 + 0xa00) = *(undefined4 *)(iVar2 + 4);
        }
      }
    }
    else {
      FUN_0016fdb0(iVar2);
      FUN_0016ef70(iVar2,6,auStack_30);
    }
    iVar5 = iRam0064f250;
    iVar7 = iRam0064def4;
    iVar2 = *(int *)(iVar2 + 0x30);
    iVar8 = iVar2 + 0x14;
    iVar9 = iRam0064def4 + -0x7c;
    iVar4 = (int)((float)iRam0064def8 * 0.15) - iVar2;
    iVar10 = iVar4 + -10;
    FUN_005109e0(iRam0064f250 + 0xa5c,0x647a20,iVar4 + -9,iRam0064def4 + -0x7b,0x23,iVar2 + 0x12,
                 0x1a);
    FUN_005109e0(iVar5 + 0xa64,0x647a38,iVar10,iVar9,0x23,iVar8,1);
    FUN_005109e0(iVar5 + 0xa6c,0x647a38,iVar10,iVar7 + -0x60,0x23,iVar8,1);
    FUN_005109e0(iVar5 + 0xa74,0x647a38,iVar10,iVar9,0x23,1,0x1c);
    FUN_005109e0(iVar5 + 0xa7c,0x647a38,iVar10 + iVar8,iVar9,0x23,1,0x1c);
  }
  return;
}

