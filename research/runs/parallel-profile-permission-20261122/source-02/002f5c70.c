
/* WARNING: Removing unreachable block (ram,0x002f63f8) */
/* WARNING: Removing unreachable block (ram,0x002f634c) */
/* WARNING: Removing unreachable block (ram,0x002f63ac) */
/* WARNING: Removing unreachable block (ram,0x002f6444) */

undefined8 FUN_002f5c70(undefined8 param_1,int param_2,undefined8 param_3)

{
  uint *puVar1;
  uint uVar2;
  undefined4 uVar3;
  int iVar4;
  int iVar5;
  ulong uVar6;
  undefined8 uVar7;
  long lVar8;
  undefined8 uVar9;
  undefined8 uVar10;
  long lVar11;
  uint uVar12;
  uint *puVar13;
  uint *puVar14;
  uint *puVar15;
  uint uVar16;
  uint uVar17;
  int *piVar18;
  undefined8 uVar19;
  float fStack_60;
  float fStack_5c;
  float fStack_58;
  float fStack_54;
  uint auStack_50 [10];
  byte bStack_28;
  byte bStack_27;
  byte bStack_26;
  byte bStack_25;
  undefined1 uStack_24;
  undefined1 uStack_23;
  undefined1 uStack_22;
  undefined1 uStack_21;
  undefined1 uStack_20;
  undefined1 uStack_1f;
  undefined1 uStack_1e;
  undefined1 uStack_1d;
  undefined1 auStack_1c [4];
  undefined1 auStack_18 [4];
  byte bStack_14;
  byte bStack_13;
  byte bStack_12;
  byte bStack_11;
  undefined4 uStack_8;
  undefined1 auStack_4 [4];
  
  iVar5 = (int)param_3;
  piVar18 = *(int **)(iVar5 + 0x34);
  if (piVar18 == (int *)0x0) {
    return 0;
  }
  iVar4 = *(int *)(iVar5 + 0xc);
  if (iVar4 == 0x13c1) {
    lVar11 = FUN_004c9820(piVar18,0);
    if (lVar11 != 4) {
      if (lVar11 != 2) {
        if (lVar11 != 1) {
          if (lVar11 != 0) {
            return 0;
          }
          FUN_001225d0(3,0x3dc200);
        }
        FUN_001225d0(3,0x152a40);
      }
      FUN_001225d0(3,0x1529a0);
    }
    FUN_001225d0(3,0x1529f0);
    return 0;
  }
  if (iVar4 == 0x7e3) {
    if (iRam00794ac8 < 0) {
      iVar5 = 0;
    }
    else {
      iVar5 = *(int *)(iRam00794ac8 * 4 + 0x794acc);
    }
    if (iVar5 != 0) {
      FUN_004c9230();
      return 1;
    }
    return 1;
  }
  if (iVar4 == 0x7e4) {
    if (iRam00794ac8 < 0) {
      iVar5 = 0;
    }
    else {
      iVar5 = *(int *)(iRam00794ac8 * 4 + 0x794acc);
    }
    if (iVar5 != 0) {
      FUN_004c91b0();
      return 1;
    }
    return 1;
  }
  if (iVar4 == 0x36b3) {
    uVar10 = FUN_004c9820(piVar18,0);
    FUN_00153040(uVar10);
    return 1;
  }
  if (iVar4 == 0x36b2) {
    FUN_00153030();
    return 1;
  }
  if (iVar4 == 0x1f63) {
    iVar5 = FUN_004c9820(piVar18,0);
    FUN_0018a1e0(iVar5 + -1);
    return 1;
  }
  if (iVar4 == 14000) {
    FUN_001531f0();
    return 1;
  }
  if (iVar4 == 12000) {
    FUN_002952f0();
    return 1;
  }
  if (iVar4 == 0x1f54) {
    return 1;
  }
  if (iVar4 == 13000) {
    FUN_00294cd0();
    return 1;
  }
  if (iVar4 == 0x1f55) {
    return 1;
  }
  if (iVar4 == 0x2c88) {
    FUN_00294d20();
    lVar11 = FUN_0012ed10(0x2001);
    if (lVar11 != 0) {
      lVar11 = FUN_0012e930(lVar11);
      if (lVar11 != 0) {
        FUN_0012bde0(0x3f800000,*(undefined4 *)((int)lVar11 + 8),0x1253c);
      }
      return 1;
    }
    return 1;
  }
  if (iVar4 == 0xff0007) {
    uVar10 = FUN_004c9ad0(piVar18,0);
    FUN_00241b80(uVar10);
    return 1;
  }
  if (iVar4 == 60000) {
    lVar11 = FUN_004c9820(piVar18,0);
    if (lVar11 == 0) {
      return 1;
    }
    if (lVar11 == 2) {
      FUN_004c87e0(*(undefined4 *)(iVar5 + 0x30),param_3,0);
      return 1;
    }
    return 1;
  }
  if (iVar4 == 0x1f52) {
    return 1;
  }
  if (iVar4 == 0x1f53) {
    return 1;
  }
  if (iVar4 == 0x1f51) {
    return 1;
  }
  if (iVar4 == 0x1f50) {
    if ((bRam00660084 & 1) == 0) {
      lVar11 = FUN_00245370();
      if (lVar11 != 0) {
        FUN_004c87e0(*(undefined4 *)(iVar5 + 0x30),param_3,0);
      }
      return 1;
    }
    return 1;
  }
  if (iVar4 == 0x1f4e) {
    FUN_00189880(1,0);
    return 1;
  }
  if (iVar4 == 0x1f4d) {
    FUN_00189880(0,0);
    return 1;
  }
  if (iVar4 == 0x1f4a) {
    FUN_00170020();
    return 1;
  }
  if (iVar4 == 0x1f49) {
    FUN_00170060();
    return 1;
  }
  if (iVar4 == 0x1f46) {
    FUN_00245920();
    return 1;
  }
  if (iVar4 == 0x1f47) {
    FUN_00245590();
    return 1;
  }
  if (iVar4 == 0x1b5a) {
    FUN_00514580();
    return 1;
  }
  if (iVar4 == 0x1b5b) {
    FUN_005144d0();
    return 1;
  }
  if (iVar4 == 0x1b59) {
    FUN_00514600();
    return 1;
  }
  if (iVar4 == 0x1f48) {
    if ((bRam00660084 & 1) != 0) {
      if ((long)((ulong)bRam00660084 << 0x3e) < 0) {
        FUN_004c87e0(*(undefined4 *)(iVar5 + 0x30),param_3,0);
      }
      return 1;
    }
    return 1;
  }
  if (iVar4 == 0x1f44) {
    lVar11 = FUN_001889a0();
    if (lVar11 != 0) {
      FUN_004c87e0(*(undefined4 *)(iVar5 + 0x30),param_3,0);
      return 1;
    }
    return 1;
  }
  if (iVar4 == 1) {
    uVar3 = *(undefined4 *)(*(int *)(iVar5 + 0x30) + 0x1c);
    uVar10 = FUN_004c9820(piVar18,0);
    uVar10 = FUN_004ca480(uVar3,uVar10);
    uVar9 = FUN_004c98e0(piVar18,1,auStack_1c);
    iVar5 = FUN_004e0f20(DAT_0064dd60,uVar9);
    lVar11 = FUN_004c9820(piVar18,2);
    if (lVar11 == 0) {
      FUN_004c1430(uVar10,(int)((float)*(int *)(*(int *)((int)uVar10 + 0x14) + 4) *
                               ((float)iVar5 / 100.0)));
    }
    else {
      FUN_004c1380(uVar10);
    }
    return 1;
  }
  if (iVar4 == 0x2ee1) {
    iVar5 = *(int *)(iVar5 + 0x2c) + -1;
    if (-1 < iVar5) {
      DAT_0064c38c = *(undefined4 *)(iVar5 * 0x48 + 0x66012c);
    }
    FUN_00188ef0(0,*(undefined4 *)(iVar5 * 0x48 + 0x66012c));
    return 1;
  }
  if (iVar4 == 0x1f43) {
    iVar5 = *(int *)(iVar5 + 0x2c) + -1;
    if (-1 < iVar5) {
      DAT_0064c38c = *(undefined4 *)(iVar5 * 0x48 + 0x66012c);
    }
    FUN_00188ef0(iVar5,*(undefined4 *)(iVar5 * 0x48 + 0x66012c));
    return 1;
  }
  if ((iVar4 == 0x1f57) || (iVar4 == 0x1f56)) {
    FUN_00185cc0(1);
    *(undefined4 *)((iVar4 + -0x1f56) * 0x48 + 0x660134) = 1;
    FUN_0012bf10(0x1252a);
    FUN_00188720(iVar4 + -0x1f56);
    return 1;
  }
  if (iVar4 == 0x1389) {
    lVar11 = FUN_004c9820(piVar18,0);
    uVar10 = 8;
    iVar5 = *(int *)(iVar5 + 0x2c) + -1;
    if ((lVar11 == 7) && (lVar8 = FUN_00185cb0(), lVar8 == 0xc)) {
      if (-1 < iVar5) {
        iRam0064e080 = *(int *)(iVar5 * 0x48 + 0x66012c);
      }
    }
    else if (-1 < iVar5) {
      iRam0064e080 = iVar5;
    }
    if (lVar11 == 0x13) {
      iRam0064e080 = DAT_0064ccd0;
    }
    FUN_002f5b10(iRam0064e080,lVar11);
    if ((undefined **)*piRam0064df54 != &PTR_LAB_0059fc70) {
      if (((((lVar11 == 9) || (lVar11 == 6)) || (lVar11 == 5)) &&
          ((lVar11 = FUN_00185cb0(), lVar11 != 0 && (lVar11 = FUN_00185cb0(), lVar11 != 1)))) &&
         (lVar11 = FUN_00185cb0(), lVar11 != 4)) {
        uVar10 = 3;
      }
      FUN_00184500(uVar10,1);
    }
    uRam0064df58 = 0x40000000;
    uRam0064eb54 = 1;
    (**(code **)(*piRam0064df54 + 0x14))();
    FUN_004c4e20(0x794910,0);
    return 1;
  }
  if (iVar4 == 0x232a) {
    FUN_00335ff0();
    return 1;
  }
  if (iVar4 == 0x232b) {
    uVar10 = FUN_004c9820(piVar18,0);
    FUN_00336130(uVar10);
    return 1;
  }
  if (iVar4 == 0x2329) {
    uVar10 = FUN_004c9820(piVar18,1);
    FUN_00182280(uVar10);
    return 1;
  }
  if (iVar4 == 9000) {
    uVar10 = FUN_004c9820(piVar18,0);
    uVar9 = FUN_004c9920(piVar18,1);
    if (*piVar18 == 4) {
      uVar7 = FUN_004c9820(piVar18,2);
      uVar19 = FUN_004c9820(piVar18,3);
    }
    else {
      uVar19 = 0x100;
      uVar7 = uVar19;
    }
    FUN_00336300(uVar10,uVar9,uVar7,uVar19);
    return 1;
  }
  if (iVar4 == 0x1392) {
    FUN_0012ba40();
    return 1;
  }
  if (iVar4 == 0x417) {
    uVar6 = FUN_004c98e0(piVar18,0,auStack_4);
    if (uVar6 < 0x50) {
      piVar18 = (int *)(&DAT_005cb090 + (int)uVar6 * 4);
      if (((*piVar18 == 0x12533) && (lVar11 = FUN_0012ed10(0x2001), lVar11 != 0)) &&
         (lVar11 = FUN_0012e930(lVar11), lVar11 != 0)) {
        FUN_0012bde0(0x3f800000,*(undefined4 *)((int)lVar11 + 8),0x1253c);
      }
      if (iRam0064df14 == 0) {
        iVar5 = *piVar18;
      }
      else {
        if (*piVar18 == 0x12542) {
          return 1;
        }
        iVar5 = *piVar18;
      }
      FUN_0012bf10(iVar5);
    }
    else {
      FUN_00163cd0(0x631390,uVar6);
    }
    return 1;
  }
  if (iVar4 == 11000) {
    uVar10 = FUN_004c98e0(piVar18,0,auStack_4);
    uVar9 = FUN_004c9820(piVar18,1);
    FUN_004e0560(DAT_0064dd60,0,uVar10,uVar9);
    return 1;
  }
  if (iVar4 == 0x1397) {
    iVar5 = FUN_004c9980(piVar18,0);
    if (*(int *)(iVar5 + 0x10) != 0) {
      FUN_002fb720();
      return 1;
    }
    return 1;
  }
  if (iVar4 == 0x1396) {
    iVar5 = FUN_004c9980(piVar18,0);
    if (*(int *)(iVar5 + 0x10) != 0) {
      FUN_002fc2b0();
      return 1;
    }
    return 1;
  }
  if (iVar4 == 0x13a9) {
    iVar5 = FUN_004c9980(piVar18,0);
    FUN_0026ce90(*(undefined4 *)(iVar5 + 0x8c));
    return 1;
  }
  if (iVar4 == 0x13a7) {
    uVar10 = FUN_004c9820(piVar18,0);
    FUN_0026cf00(uVar10);
    return 1;
  }
  if (iVar4 == 0x13a8) {
    FUN_0026d010();
    return 1;
  }
  if (iVar4 == 0x2afa) {
    uVar10 = FUN_004c9a40(piVar18,0);
    uVar9 = FUN_004c9ad0(piVar18,1);
    if (*(int *)(param_2 + 0x1b8) < 0) {
      iVar5 = 0;
    }
    else {
      iVar5 = *(int *)(*(int *)(param_2 + 0x1b8) * 4 + param_2 + 0x1bc);
    }
    uVar9 = FUN_00240370(*(int *)(*(int *)(iVar5 + 0x50) + 0x10) + 4,uVar9);
    (**(code **)(*(int *)uVar10 + 0x38))(uVar10,uVar9);
    return 1;
  }
  if (iVar4 == 0x2b03) {
    uVar10 = FUN_004c98e0(piVar18,0,auStack_18);
    iVar5 = FUN_004c9820(piVar18,1);
    iVar4 = FUN_004e0f20(DAT_0064dd60,uVar10);
    FUN_004e0560(DAT_0064dd60,0,uVar10,iVar4 + iVar5);
    return 1;
  }
  if (iVar4 == 0x1395) {
    uVar10 = FUN_004c9980(piVar18,0);
    FUN_004c9880(&bStack_28,piVar18,1);
    bStack_14 = bStack_28;
    bStack_13 = bStack_27;
    bStack_12 = bStack_26;
    bStack_11 = bStack_25;
    fStack_60 = (float)bStack_28 / 255.0;
    fStack_5c = (float)bStack_27 / 255.0;
    fStack_58 = (float)bStack_26 / 255.0;
    fStack_54 = (float)bStack_25 / 255.0;
    FUN_004c7590(uVar10,&fStack_60);
    return 1;
  }
  if (iVar4 == 0x1394) {
    iVar5 = FUN_004c9980(piVar18,0);
    iVar4 = FUN_004c9820(piVar18,1);
    FUN_004c9880(&uStack_24,piVar18,2);
    iVar5 = *(int *)(iVar5 + 0xc) + iVar4 * 4;
    *(undefined1 *)(iVar5 + 0x40) = uStack_24;
    *(undefined1 *)(iVar5 + 0x41) = uStack_23;
    *(undefined1 *)(iVar5 + 0x42) = uStack_22;
    *(undefined1 *)(iVar5 + 0x43) = uStack_21;
    return 1;
  }
  if (iVar4 == 0x1393) {
    iVar5 = FUN_004c9a40(piVar18,0);
    FUN_004c9880(&uStack_20,piVar18,1);
    iVar5 = *(int *)(iVar5 + 0xc);
    *(undefined1 *)(iVar5 + 0x2c) = uStack_20;
    *(undefined1 *)(iVar5 + 0x2d) = uStack_1f;
    *(undefined1 *)(iVar5 + 0x2e) = uStack_1e;
    *(undefined1 *)(iVar5 + 0x2f) = uStack_1d;
    return 1;
  }
  if (iVar4 == 0x2b01) {
    uStack_8 = 0;
    uVar10 = FUN_004c98e0(piVar18,0,&uStack_8);
    FUN_004e0fa0(DAT_0064dd60,uVar10,0x630e08);
    FUN_004c9230(*(undefined4 *)(*(int *)(iVar5 + 0x30) + 0x1c));
    return 1;
  }
  if (iVar4 == 26000) {
    uVar3 = *(undefined4 *)(iVar5 + 0x30);
  }
  else {
    if (iVar4 != 0x2af9) {
      if (iVar4 == 0x1f6f) {
        uVar10 = FUN_00153d40();
        FUN_00188ef0(0,uVar10);
        return 0;
      }
      if (iVar4 == 0x1450) {
        lVar11 = FUN_004ef1b0(0);
        if (lVar11 != 0) {
          FUN_004c87e0(*(undefined4 *)(iVar5 + 0x30),param_3,0);
          return 1;
        }
        return 1;
      }
      if (iVar4 != 0x13c3) {
        if (iVar4 != 0x13c0) {
          if (iVar4 != 0x13c2) {
            return 0;
          }
          return 1;
        }
        lVar11 = FUN_003433b0();
        if (lVar11 == 2) {
          lVar11 = FUN_004eee60(0);
          if (lVar11 != 0) {
            FUN_004c87e0(*(undefined4 *)(iVar5 + 0x30),param_3,0);
            return 1;
          }
          return 1;
        }
        return 1;
      }
      iVar5 = *(int *)(iVar5 + 0x30);
      uVar12 = 0;
      if (iVar5 == 0) {
        return 1;
      }
      piVar18 = *(int **)(*(int *)(*(int *)(iVar5 + 0x18) + 0x18) + 4);
      if (*piVar18 != 0x47524f50) {
        return 1;
      }
      piVar18 = *(int **)(*(int *)(*(int *)(piVar18[2] + 0x18) + 0x18) + 4);
      if (*piVar18 != 0x504f4c59) {
        return 1;
      }
      iVar5 = *(int *)(iVar5 + 0x1c);
      iVar4 = piVar18[2];
      puVar1 = *(uint **)(*(int *)(iVar5 + 0x54) + 0x10);
      uVar17 = 0;
      if (*puVar1 != 0) {
        puVar14 = auStack_50;
        puVar15 = puVar1;
        do {
          puVar13 = (uint *)puVar15[5];
          uVar2 = *puVar13;
          uVar16 = 0;
          if (uVar2 != 0) {
            do {
              if (*(int *)(puVar13[1] + 8) == iVar4) {
                if (9 < uVar12) {
                  return 1;
                }
                *puVar14 = uVar17;
                uVar12 = uVar12 + 1;
                puVar14 = puVar14 + 1;
                break;
              }
              uVar16 = uVar16 + 1;
              puVar13 = puVar13 + 1;
            } while (uVar16 < uVar2);
          }
          uVar17 = uVar17 + 1;
          puVar15 = puVar15 + 6;
        } while (uVar17 < *puVar1);
      }
      if (uVar12 != 0) {
        uVar12 = FUN_00185c30(uVar12 & 0xffff);
        uVar10 = FUN_004ca480(iVar5,auStack_50[uVar12 & 0xffff]);
        FUN_004c18c0(uVar10,0,1);
        return 1;
      }
      return 1;
    }
    uVar3 = *(undefined4 *)(iVar5 + 0x30);
  }
  uVar10 = FUN_004c98e0(piVar18,0,auStack_4);
  uVar9 = FUN_004c9820(piVar18,1);
  uVar7 = FUN_004c98e0(piVar18,2,auStack_4);
  uVar10 = FUN_004e0f20(DAT_0064dd60,uVar10);
  lVar11 = FUN_004bdcc0(uVar10,uVar9,uVar7);
  if (lVar11 != 0) {
    FUN_004c87e0(uVar3,param_3,0);
    return 1;
  }
  return 1;
}

