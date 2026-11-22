
/* WARNING: Removing unreachable block (ram,0x003f0dac) */

undefined4 FUN_003f0190(void)

{
  int *piVar1;
  byte bVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar10;
  int iVar11;
  long lVar6;
  long lVar7;
  long lVar8;
  long lVar9;
  int iVar12;
  int iVar13;
  int iVar14;
  int *piVar15;
  int iVar16;
  uint uVar17;
  int iVar18;
  int iVar19;
  int iVar20;
  int iVar21;
  uint uVar22;
  int iVar23;
  int iVar24;
  float fVar25;
  undefined4 uVar26;
  float fVar27;
  undefined4 *puStack_130;
  int *piStack_120;
  int *piStack_110;
  char *pcStack_f0;
  char *pcStack_e0;
  undefined1 auStack_d0 [80];
  uint uStack_80;
  uint uStack_7c;
  uint uStack_78;
  undefined4 uStack_74;
  undefined4 auStack_70 [8];
  int aiStack_50 [8];
  int aiStack_30 [11];
  int iStack_4;
  
  iVar20 = iRam0064f1ec;
  iVar4 = iRam0064def8;
  iVar5 = iRam0064def4;
  iVar23 = 0;
  iVar21 = 0;
  iVar10 = iRam0064def8 >> 0x1f;
  iVar16 = *(int *)(iRam0064f1ec + 0x18);
  iVar11 = iRam0064def4 >> 0x1f;
  iVar19 = *(int *)(iRam0066008c + 0xac);
  piVar1 = *(int **)(iRam0064f1ec + 0x14);
  pcStack_e0 = (char *)FUN_00215810(2,2);
  pcStack_f0 = (char *)FUN_00215810(2,7);
  iVar14 = *(int *)(iVar20 + 0xea8);
  iVar12 = *(int *)(iVar20 + 0xeb4);
  if ((pcStack_e0 == (char *)0x0) || (*pcStack_e0 == '\0')) {
    pcStack_e0 = s_WINS__0063fc38;
  }
  if ((pcStack_f0 == (char *)0x0) || (*pcStack_f0 == '\0')) {
    pcStack_f0 = s_PLAYER__d_0063fc50;
  }
  piStack_110 = aiStack_30;
  iVar18 = 0;
  piStack_120 = aiStack_50;
  puStack_130 = auStack_70;
  iVar24 = iVar20;
  do {
    iVar3 = *(int *)(iVar24 + 0x24c);
    if (iVar3 == 0) {
      iVar3 = 0;
    }
    else if (*(int *)(iVar3 + 4) != *(int *)(iVar24 + 0x250)) {
      iVar3 = 0;
    }
    if (iVar3 == 0) {
      *piStack_110 = -1;
    }
    else {
      if (iVar12 < 1) {
        iVar13 = *(int *)(iVar3 + 0x3f4);
        *(undefined4 *)(iVar3 + 0xc4) = 0x47c35000;
      }
      else {
        iVar13 = *(int *)(iVar3 + 0x3f4);
        if (iVar13 == 0) {
          iVar13 = *(int *)(iVar3 + 0x3f8);
        }
        uVar26 = FUN_00155d10(iVar3 + 100,iVar14 + iVar13 * 0x60 + 0x30);
        *(undefined4 *)(iVar3 + 0xc4) = uVar26;
      }
      *piStack_110 = iVar18;
      *piStack_120 = iVar13 + *(int *)(iVar20 + 0xeb4) * *(int *)(iVar3 + 0xb8);
      *puStack_130 = *(undefined4 *)(iVar3 + 0xc4);
    }
    iVar18 = iVar18 + 1;
    piStack_110 = piStack_110 + 1;
    piStack_120 = piStack_120 + 1;
    puStack_130 = puStack_130 + 1;
    iVar24 = iVar24 + 8;
  } while (iVar18 < 8);
  if (*(int *)(iVar19 + 0x40) == 1) {
    FUN_003f5710(aiStack_30,aiStack_50,auStack_70);
  }
  iVar12 = 0;
  piVar15 = aiStack_30;
  iVar14 = iVar20;
  do {
    if (*(int *)(iVar19 + 0x40) == 1) {
      *(int *)(iVar14 + 0x29c) = *piVar15;
    }
    else {
      *(int *)(iVar14 + 0x29c) = iVar12;
    }
    iVar12 = iVar12 + 1;
    piVar15 = piVar15 + 1;
    iVar14 = iVar14 + 4;
  } while (iVar12 < 8);
  iVar14 = 0;
  iVar19 = iVar20;
  do {
    if (-1 < *(int *)(iVar19 + 0x29c)) {
      iVar24 = iVar20 + *(int *)(iVar19 + 0x29c) * 8;
      iVar12 = *(int *)(iVar24 + 0x24c);
      if (iVar12 == 0) {
        iVar12 = 0;
      }
      else if (*(int *)(iVar12 + 4) != *(int *)(iVar24 + 0x250)) {
        iVar12 = 0;
      }
      if (iVar12 != 0) {
        iVar24 = *(int *)(iVar12 + 0x250);
        if (iVar24 == 0) {
          iVar24 = 0;
        }
        else if (*(int *)(iVar24 + 4) != *(int *)(iVar12 + 0x254)) {
          iVar24 = 0;
        }
        if ((iVar24 != 0) && (lVar6 = FUN_0012e930(), lVar6 != 0)) {
          if (*(int *)(iVar20 + 0x29c) == *(int *)(iVar12 + 0x20)) {
            if (*(int *)(iVar12 + 0x1b8) == 1) {
              if (iRam0064e914 == 1) {
                iRam006ac338 = iRam006ac338 + 1;
              }
            }
            else if ((*(int *)(iVar12 + 0x1b8) == 0) && (iRam0064e920 == 1)) {
              iRam006aff68 = iRam006aff68 + 1;
            }
          }
          else if (*(int *)(iVar12 + 0x1b8) == 1) {
            if (iRam0064e914 == 1) {
              iRam006ac33c = iRam006ac33c + 1;
            }
          }
          else if ((*(int *)(iVar12 + 0x1b8) == 0) && (iRam0064e920 == 1)) {
            iRam006aff6c = iRam006aff6c + 1;
          }
          if (*(int *)(iVar20 + 0x6c) < 2) {
            iVar18 = 0;
            iVar24 = iVar20;
            do {
              if (*(int *)(iVar24 + 0x29c) == *(int *)(iVar12 + 0x20)) {
                iVar23 = *(int *)(iVar12 + 0x20);
                iVar21 = iVar18;
              }
              iVar18 = iVar18 + 1;
              iVar24 = iVar24 + 4;
            } while (iVar18 < 8);
          }
        }
      }
    }
    iVar14 = iVar14 + 1;
    iVar19 = iVar19 + 4;
  } while (iVar14 < 8);
  iVar19 = *(int *)(iVar20 + 0x28c);
  if (iVar19 == 0) {
    iVar19 = 0;
  }
  else if (*(int *)(iVar19 + 4) != *(int *)(iVar20 + 0x290)) {
    iVar19 = 0;
  }
  if (iVar19 == 0) {
    FUN_00129bb0();
    iVar16 = 0;
    do {
      iVar19 = *(int *)(iVar20 + 0x24c);
      if (iVar19 == 0) {
        iVar19 = 0;
      }
      else if (*(int *)(iVar19 + 4) != *(int *)(iVar20 + 0x250)) {
        iVar19 = 0;
      }
      if (iVar19 == 0) {
        FUN_0041a580(0,6);
      }
      iVar16 = iVar16 + 1;
      iVar20 = iVar20 + 8;
    } while (iVar16 < 8);
  }
  else {
    iVar14 = *(int *)(*(int *)(iVar19 + 0x2c) + 0x10);
    iVar12 = iRam0064def8 >> 1;
    lVar6 = FUN_00220420();
    if (lVar6 != 0) {
      iVar12 = iVar12 + *(int *)(iVar16 + 0xa8);
    }
    fVar25 = fRam0064de94 * 120.0;
    if (fVar25 < 2.1474836e+09) {
      uVar22 = (uint)fVar25;
      iVar24 = *(int *)(iVar16 + 4);
    }
    else {
      uVar22 = (int)(fVar25 - 2.1474836e+09) | 0x80000000;
      iVar24 = *(int *)(iVar16 + 4);
    }
    iVar24 = iRam0064def4 - *(int *)(iVar24 + 0x724);
    if (*(int *)(*(int *)(iVar19 + 0x20) * 4 + iVar20 + 0x140) == 0) {
      FUN_001de4b0(0x7b9130,pcStack_f0,*(int *)(iVar19 + 0x20) + 1);
    }
    else {
      FUN_001de4b0(0x7b9130,0x63fc48);
    }
    lVar6 = FUN_0016f670(0x603d,0xb,0x7b9130,iVar12,iVar24 + 0x10,0x36);
    FUN_001de4b0(0x7b9130,pcStack_e0);
    lVar7 = FUN_0016f670(0x603e,0xb,0x7b9130,iVar12,
                         (iRam0064def4 - *(int *)(*(int *)(iVar16 + 4) + 0x740)) + 0x10,0x36);
    iVar12 = (int)lVar7;
    if ((lVar6 != 0) && (lVar7 != 0)) {
      iVar24 = *(int *)(*(int *)(iRam0064f1ec + 0x18) + 4);
      lVar8 = FUN_00172d80(0x1300d4,0x602e,*(undefined4 *)(iVar24 + 0x6e4),0,0x3c);
      iVar18 = (int)lVar8;
      if (lVar8 == 0) {
        iVar12 = *(int *)(iVar19 + 0x250);
      }
      else {
        iVar3 = *(int *)(*(int *)(iVar18 + 0x10) + 0x10);
        iVar13 = iRam0064def4 - *(int *)(iVar24 + 0x6ec);
        *(undefined4 *)(iVar18 + 0x14) = *(undefined4 *)(iVar24 + 0x6e8);
        *(int *)(iVar18 + 0x18) = (iVar13 - iVar3) + 0x10;
        *(undefined4 *)(iVar18 + 0x28) = *(undefined4 *)(iVar24 + 0x6f0);
        *(undefined4 *)(iVar18 + 0x2c) = *(undefined4 *)(iVar24 + 0x6f4);
        lVar9 = FUN_0012dda0(0x20ac,0x20,0x3ef920,0x30,&iStack_4);
        if (lVar9 != 0) {
          iVar24 = (int)lVar6;
          if ((lVar7 != 0) && (lVar6 != 0)) {
            uStack_80 = (uint)DAT_0064dce8;
            uStack_7c = (uint)DAT_0064dce9;
            uStack_78 = (uint)DAT_0064dcea;
            uStack_74 = 0;
            FUN_002eb9b0(iVar12 + 0x3c,&uStack_80);
            FUN_002eb9b0(iVar24 + 0x3c,&uStack_80);
            if (lVar6 == 0) {
              *(undefined4 *)(iStack_4 + 0x18) = 0;
              *(undefined4 *)(iStack_4 + 0x1c) = 0;
            }
            else {
              *(int *)(iStack_4 + 0x18) = iVar24;
              *(undefined4 *)(iStack_4 + 0x1c) = *(undefined4 *)(iVar24 + 4);
            }
            if (lVar7 == 0) {
              *(undefined4 *)(iStack_4 + 0x10) = 0;
              *(undefined4 *)(iStack_4 + 0x14) = 0;
            }
            else {
              *(int *)(iStack_4 + 0x10) = iVar12;
              *(undefined4 *)(iStack_4 + 0x14) = *(undefined4 *)(iVar12 + 4);
            }
          }
          *(undefined4 *)(iVar18 + 0x28) = 0x3e800000;
          *(undefined4 *)(iVar18 + 0x2c) = 0x3e800000;
          if (*(int *)(iVar20 + 0x6c) < 2) {
            uVar26 = *(undefined4 *)(iVar18 + 0x14);
          }
          else {
            iVar3 = *(int *)(*(int *)(iVar18 + 0x34) + 0x54);
            if (iVar3 < 0) {
              iVar3 = iVar3 + 1;
            }
            if (iVar10 < 0) {
              iVar4 = iVar4 + 1;
            }
            *(int *)(iVar18 + 0x14) = (iVar4 >> 1) - (iVar3 >> 1);
            iVar4 = *(int *)(*(int *)(iVar18 + 0x34) + 0x58);
            if (iVar4 < 0) {
              iVar4 = iVar4 + 1;
            }
            if (iVar11 < 0) {
              iVar5 = iVar5 + 1;
            }
            *(int *)(iVar18 + 0x18) = (iVar5 >> 1) - (iVar4 >> 1);
            if ((lVar7 != 0) && (lVar6 != 0)) {
              *(int *)(iVar24 + 0x2c) = *(int *)(iVar18 + 0x18) + 0x5f;
              *(int *)(iVar12 + 0x2c) = *(int *)(iVar18 + 0x18) + -5;
            }
            uVar26 = *(undefined4 *)(iVar18 + 0x14);
          }
          *(undefined4 *)(iStack_4 + 0x20) = uVar26;
          *(undefined4 *)(iStack_4 + 0x24) = *(undefined4 *)(iVar18 + 0x18);
          *(int *)(iStack_4 + 0x28) = iVar21;
          *(int *)(iStack_4 + 0x2c) = iVar23;
          if (lVar8 == 0) {
            *(undefined4 *)(iStack_4 + 8) = 0;
            *(undefined4 *)(iStack_4 + 0xc) = 0;
          }
          else {
            *(int *)(iStack_4 + 8) = iVar18;
            *(undefined4 *)(iStack_4 + 0xc) = *(undefined4 *)(iVar18 + 4);
          }
        }
        iVar12 = *(int *)(iVar19 + 0x250);
      }
      iVar5 = -1;
      if (iVar12 == 0) {
        iVar12 = 0;
      }
      else if (*(int *)(iVar12 + 4) != *(int *)(iVar19 + 0x254)) {
        iVar12 = 0;
      }
      if (((iVar12 != 0) && (lVar6 = FUN_0012e930(), lVar6 != 0)) &&
         (lVar6 = FUN_00181870(*(undefined4 *)((int)lVar6 + 0x90)), lVar6 != 0)) {
        iVar5 = *(int *)(iVar19 + 0x20);
      }
      if (iVar5 < 0) {
        FUN_00129bb0();
      }
      else {
        FUN_00129a10();
      }
      iVar12 = 0;
      iVar19 = iVar20;
      do {
        iVar4 = *(int *)(iVar19 + 0x24c);
        if (iVar4 == 0) {
          iVar4 = 0;
        }
        else if (*(int *)(iVar4 + 4) != *(int *)(iVar19 + 0x250)) {
          iVar4 = 0;
        }
        if (iVar4 != 0) {
          if (*(int *)(iVar4 + 0x20) == iVar5) {
            FUN_0041a580(iVar4,5);
          }
          else {
            FUN_0041a580(iVar4,6);
          }
        }
        iVar12 = iVar12 + 1;
        iVar19 = iVar19 + 8;
      } while (iVar12 < 8);
      uVar17 = 0;
      if (uVar22 != 0) {
        do {
          FUN_003f3c30();
          FUN_00413a00();
          uRam0064df58 = 0x3f800000;
          (**(code **)(*piRam0064df54 + 0x14))();
          uVar17 = uVar17 + 1;
        } while (uVar17 < uVar22);
      }
      if (iVar14 != -1) {
        FUN_0012bf10(iVar14);
      }
      iVar19 = *(int *)(*(int *)(iRam0064f1ec + 0x18) + 4);
      lVar6 = FUN_00172d80(0x1300d4,0x602e,*(undefined4 *)(iVar19 + 0x6c8),0,0x3c);
      iVar14 = (int)lVar6;
      if (lVar6 != 0) {
        iVar12 = *(int *)(*(int *)(iVar14 + 0x10) + 0x10);
        iVar5 = iRam0064def4 - *(int *)(iVar19 + 0x6d0);
        *(undefined4 *)(iVar14 + 0x14) = *(undefined4 *)(iVar19 + 0x6cc);
        *(int *)(iVar14 + 0x18) = (iVar5 - iVar12) + 0x10;
        *(undefined4 *)(iVar14 + 0x28) = *(undefined4 *)(iVar19 + 0x6d4);
        *(undefined4 *)(iVar14 + 0x2c) = *(undefined4 *)(iVar19 + 0x6d8);
        *(byte *)(iVar14 + 0xc) = *(byte *)(iVar14 + 0xc) & 0xef | 0x10;
        fVar25 = *(float *)(iVar14 + 0x2c);
        iVar19 = *(int *)(iVar14 + 0x14);
        *(undefined4 *)(iVar14 + 0x2c) = 0x3f800000;
        *(int *)(iVar14 + 0x14) = -*(int *)(*(int *)(iVar14 + 0x10) + 0xc);
        if (*(float *)(iVar14 + 0x2c) < fVar25) {
          iVar12 = *(int *)(iVar14 + 0x14);
          while( true ) {
            if (iVar12 < iVar19) {
              iVar12 = iVar12 + *(int *)(iVar16 + 0x9c);
              *(int *)(iVar14 + 0x14) = iVar12;
              if (iVar19 < iVar12) {
                *(int *)(iVar14 + 0x14) = iVar19;
              }
            }
            else {
              fVar27 = *(float *)(iVar14 + 0x2c) + *(float *)(iVar16 + 0xa0);
              *(float *)(iVar14 + 0x2c) = fVar27;
              if (fVar25 < fVar27) {
                *(float *)(iVar14 + 0x2c) = fVar25;
              }
            }
            FUN_003f3c30();
            FUN_00413a00();
            uRam0064df58 = 0x3f800000;
            (**(code **)(*piRam0064df54 + 0x14))();
            if (fVar25 <= *(float *)(iVar14 + 0x2c)) break;
            iVar12 = *(int *)(iVar14 + 0x14);
          }
        }
      }
      uVar22 = 0;
      uVar26 = *(undefined4 *)(*(int *)(iVar16 + 4) + 0x790);
      iVar14 = (iRam0064def4 - *(int *)(*(int *)(iVar16 + 4) + 0x794)) + 0x10;
      iVar19 = iVar20;
      do {
        iVar12 = *(int *)(iVar19 + 0x29c);
        bVar2 = 0;
        if (iVar12 < 0) {
          iVar12 = *(int *)(iVar16 + 0xa4);
        }
        else {
          if (*(int *)(iVar20 + iVar12 * 4 + 0x140) == 0) {
            FUN_001de4b0(auStack_d0,0x63fc40,uVar22 + 1,pcStack_f0);
            FUN_001de4b0(0x7b9130,auStack_d0,iVar12 + 1);
          }
          else {
            FUN_001de4b0(0x7b9130,0x63fc40,uVar22 + 1);
          }
          lVar6 = FUN_0016f990(0x603f,0x11,0x7b9130,uVar26,iVar14,0x36);
          if (lVar6 == 0) break;
          iVar5 = iVar20 + iVar12 * 8;
          iVar12 = *(int *)(iVar5 + 0x24c);
          if (iVar12 == 0) {
            iVar12 = 0;
          }
          else if (*(int *)(iVar12 + 4) != *(int *)(iVar5 + 0x250)) {
            iVar12 = 0;
          }
          if ((iVar12 == 0) ||
             (lVar7 = FUN_00172d80(0x1300d4,0x602e,
                                   *(undefined4 *)(*piVar1 + *(int *)(iVar12 + 0x1c) * 0x38 + 0x34),
                                   0,0x3b), lVar7 == 0)) break;
          iVar12 = *(int *)(iVar16 + 4);
          iVar5 = 0;
          *(undefined4 *)((int)lVar7 + 0x14) = *(undefined4 *)(iVar12 + 0x758);
          *(uint *)((int)lVar7 + 0x18) =
               ((iRam0064def4 - *(int *)(iVar12 + 0x75c)) + 0x10) - uVar22 * *(int *)(iVar16 + 0xa4)
          ;
          do {
            FUN_00183b30(lVar6,bVar2);
            FUN_001845e0((float)bVar2,lVar7);
            bVar2 = bVar2 + 0xc;
            FUN_003f3c30();
            FUN_00413a00();
            uRam0064df58 = 0x3f800000;
            (**(code **)(*piRam0064df54 + 0x14))();
            iVar5 = iVar5 + 1;
          } while (iVar5 < 0x14);
          FUN_00183b30(lVar6,0xff);
          FUN_001845e0(0x437f0000,lVar7);
          iVar12 = *(int *)(iVar16 + 0xa4);
        }
        iVar14 = iVar14 - iVar12;
        uVar22 = uVar22 + 1;
        iVar19 = iVar19 + 4;
      } while (uVar22 < 8);
    }
  }
  iVar16 = 0;
  do {
    FUN_003f3c30();
    FUN_00413a00();
    uRam0064df58 = 0x3f800000;
    (**(code **)(*piRam0064df54 + 0x14))();
    iVar16 = iVar16 + 1;
  } while (iVar16 < 0xf0);
  FUN_002816f0();
  iVar16 = 0;
  do {
    FUN_00183330(iVar16,2);
    iVar16 = iVar16 + 1;
  } while (iVar16 < 2);
  FUN_00185cc0(0xc);
  FUN_001225d0(10,0x52bdd0);
  FUN_001225d0(10,0x5200d0);
  FUN_001225d0(0,0x18e100);
  return 0xbf800000;
}

