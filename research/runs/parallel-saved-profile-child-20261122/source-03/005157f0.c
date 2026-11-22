
undefined4 FUN_005157f0(int param_1)

{
  undefined4 uVar1;
  long lVar2;
  undefined8 uVar3;
  int iVar4;
  uint uVar5;
  int iVar6;
  byte *pbVar7;
  int iVar8;
  int iVar9;
  float fVar10;
  undefined1 auStack_c0 [144];
  float fStack_30;
  float fStack_2c;
  float fStack_28;
  float fStack_20;
  float fStack_1c;
  float fStack_18;
  float fStack_10;
  float fStack_c;
  float fStack_8;
  
  uVar5 = 0;
  iVar4 = 0x7bd230;
  do {
    if (*(uint *)(iVar4 + 0xac) == (*(uint *)(param_1 + 0xc) | 0x10000000)) {
      iVar4 = uVar5 * 0xa28 + 0x7bd2dc;
      goto LAB_00515874;
    }
    uVar5 = uVar5 + 1;
    iVar4 = iVar4 + 0xa28;
  } while (uVar5 < 4);
  iVar4 = 0;
LAB_00515874:
  if (iVar4 == 0) {
    uVar1 = 0;
  }
  else {
    uVar5 = 0;
    if (*(int *)(iVar4 + 0x9f4) != 0) {
      uVar1 = *(undefined4 *)(param_1 + 0xb24);
      while( true ) {
        pbVar7 = (byte *)(iVar4 + uVar5 + 0x9f8);
        lVar2 = FUN_00215df0(0x647b10,0,uVar1);
        if (lVar2 != 0) {
          iVar6 = (int)lVar2;
          iVar8 = iVar6 + ((uint)*pbVar7 * 0xc + (uint)*pbVar7) * 4;
          iVar9 = *(int *)(iVar8 + 0xc);
          iRam0064f298 = param_1;
          if (iVar9 == 3) {
            uVar3 = FUN_00436d30(*(undefined4 *)(param_1 + 0x10));
            lVar2 = FUN_00443430(*(undefined4 *)(iVar8 + 0x10),0x3f266666,
                                 *(undefined4 *)(iVar8 + 0x18),*(undefined4 *)(iVar8 + 0x1c),uVar3,
                                 *(undefined4 *)(iVar8 + 4),0,*(undefined4 *)(iVar8 + 0x20),0,
                                 *(undefined4 *)(iVar8 + 8));
            if (lVar2 != 0) {
              *(uint *)((int)lVar2 + 0x80) = iVar6 + ((uint)*pbVar7 * 0xc + (uint)*pbVar7) * 4;
            }
          }
          else if (iVar9 == 0) {
            *(undefined **)(iVar8 + 4) = &DAT_005eda80;
            iVar9 = iVar6 + ((uint)*pbVar7 * 0xc + (uint)*pbVar7) * 4;
            uVar3 = FUN_00436d30(*(undefined4 *)(param_1 + 0x10));
            lVar2 = FUN_00443420(*(undefined4 *)(iVar9 + 0x10),0x3eb33333,
                                 *(undefined4 *)(iVar9 + 0x18),*(undefined4 *)(iVar9 + 0x1c),uVar3,
                                 *(undefined4 *)(iVar9 + 4),0,*(undefined4 *)(iVar9 + 0x20));
            if (lVar2 != 0) {
              *(uint *)((int)lVar2 + 0x80) = iVar6 + ((uint)*pbVar7 * 0xc + (uint)*pbVar7) * 4;
            }
          }
          else if (iVar9 - 1U < 2) {
            uVar3 = FUN_00436d30(*(undefined4 *)(param_1 + 0x10));
            lVar2 = FUN_00443420(*(undefined4 *)(iVar8 + 0x10),0,*(undefined4 *)(iVar8 + 0x18),
                                 *(undefined4 *)(iVar8 + 0x1c),uVar3,*(undefined4 *)(iVar8 + 4),0,
                                 *(undefined4 *)(iVar8 + 0x20));
            if (lVar2 != 0) {
              fVar10 = 0.0;
              iVar9 = (int)lVar2;
              *(uint *)(iVar9 + 0x80) = iVar6 + ((uint)*pbVar7 * 0xc + (uint)*pbVar7) * 4;
              if (0.0 < (float)(*(uint **)(iVar9 + 8))[10]) {
                fStack_10 = *(float *)(iVar9 + 0x10);
                fStack_c = *(float *)(iVar9 + 0x14);
                fStack_8 = *(float *)(iVar9 + 0x18);
                if ((**(uint **)(iVar9 + 8) & 0x40000) == 0) {
                  fVar10 = (float)FUN_00445a20(*(undefined4 *)(iVar9 + 0x28),&fStack_10,1);
                }
                fStack_c = fStack_c + fVar10;
                if ((long)((ulong)*(byte *)(iVar9 + 0x34) << 0x39) < 0) {
                  FUN_0042d1c0(*(undefined4 *)(*(int *)(iVar9 + 8) + 0x28),0x40a00000,
                               *(float *)(*(int *)(iVar9 + 8) + 0x2c) * *(float *)(iVar9 + 0x68),
                               *(undefined4 *)(iVar9 + 0x1c),auStack_c0,&fStack_10);
                }
                else {
                  FUN_0042d1c0(*(undefined4 *)(*(int *)(iVar9 + 8) + 0x28),0x40a00000,
                               *(undefined4 *)(*(int *)(iVar9 + 8) + 0x2c),
                               *(undefined4 *)(iVar9 + 0x1c),auStack_c0,&fStack_10);
                }
                if (((ulong)*(byte *)(iVar9 + 0x34) & 3) >> 1 == 1) {
                  FUN_00156220(*(undefined4 *)(iVar9 + 0x1c),&fStack_20);
                  fStack_20 = fStack_20 * -0.75;
                  fStack_18 = fStack_18 * -0.75;
                  fStack_30 = fStack_20 + fStack_10;
                  fStack_2c = fStack_1c + fStack_c;
                  fStack_28 = fStack_18 + fStack_8;
                  FUN_0042d1c0(*(undefined4 *)(*(int *)(iVar9 + 8) + 0x28),0x40a00000,
                               *(float *)(*(int *)(iVar9 + 8) + 0x2c) * 1.5,
                               *(undefined4 *)(iVar9 + 0x1c),auStack_c0,&fStack_30);
                }
                iVar9 = *(int *)(iVar9 + 0x28);
                uVar3 = FUN_00451b90(iVar9,3,0);
                FUN_0019d640(auStack_c0,uVar3,*(undefined4 *)(iVar9 + 0xf9c));
              }
            }
          }
          iRam0064f298 = 0;
        }
        uVar5 = uVar5 + 1;
        if (*(uint *)(iVar4 + 0x9f4) <= uVar5) break;
        uVar1 = *(undefined4 *)(param_1 + 0xb24);
      }
    }
    uVar1 = 1;
  }
  return uVar1;
}

