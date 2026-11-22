
undefined4 FUN_00151dc0(int param_1)

{
  char *pcVar1;
  undefined4 uVar2;
  int iVar3;
  long lVar4;
  long lVar5;
  int iVar6;
  float fVar7;
  float fVar8;
  float fVar9;
  float fVar10;
  
  if ((iGpffff9db4 == 7) && (lVar4 = FUN_00127100(), 1 < lVar4)) {
    if (param_1 < 0x2b) {
      return 0;
    }
    if (0x2f < param_1) {
      return 0;
    }
    param_1 = param_1 + 5;
  }
  FUN_001a8380(0x3f800000);
  FUN_00151c50(param_1);
  uGpffff9f84 = 0;
  uRam006600e4 = 0;
  uGpffff9f88 = 0;
  uRam006600e0 = 0;
  uGpffff9f44 = 0;
  uRam00660114 = 0;
  uRam0065df34 = 0;
  uGpffff9f48 = 0;
  if ((piRam0066008c == (int *)0x0) || ((piRam0066008c[0x22] & 1U) == 0)) {
    lVar4 = FUN_00152660(param_1);
    if (lVar4 < 0) {
      uVar2 = 0;
    }
    else {
      iVar6 = *piRam0066008c;
      if ((piRam0066008c != (int *)0x0) && (iVar6 != 0)) {
        if ((iGpffff9db4 == 9) && (iRam007bfc44 == 6)) {
          FUN_0026f070(uRam007bfcc4);
        }
        if ((iGpffff9db4 == 10) || (iGpffff9db4 == 6)) {
          FUN_0019b7c0(lVar4,iVar6,1);
        }
        else if (param_1 == 0x17) {
          lVar5 = FUN_00220420();
          if ((lVar5 != 0) && (lVar5 = FUN_001df5a0(iVar6,0x5f8d78), lVar5 != 0)) {
            *(undefined1 *)((int)lVar5 + -1) = 0x77;
          }
          FUN_0019b7c0(lVar4,iVar6,1);
        }
        else {
          if (iGpffff9db4 == 7) {
            lVar5 = FUN_00127100();
            if (lVar5 < 2) {
              iVar6 = *(int *)(piRam0066008c[0x2b] + 4);
            }
            else {
              iVar6 = *(int *)(piRam0066008c[0x2b] + 8);
            }
          }
          FUN_0019a740(lVar4,iVar6,1);
        }
        FUN_0019b3e0(lVar4);
        FUN_0026f070((&PTR_PTR_005a0a00)[param_1 * 6]);
      }
      if ((*(int *)(iRam00660090 + 0x4c) != 0) && (lVar5 = FUN_00130700(&gp0xffff9f10), lVar5 == 0))
      {
        FUN_001280d0(*(undefined4 *)(iRam00660090 + 0x4c),&gp0xffff9f10);
      }
      if (*(int *)(iRam00660090 + 0x50) != 0) {
        lVar5 = FUN_00130700(&gp0xffff9f24);
        if (lVar5 == 0) {
          FUN_001280d0(*(undefined4 *)(iRam00660090 + 0x50),&gp0xffff9f24);
        }
        if (iGpffff9db4 == 7) {
          lVar5 = FUN_00130700(&gp0xffff9f28);
          if (lVar5 == 0) {
            FUN_001280d0(*(undefined4 *)(iRam00660090 + 0x50),&gp0xffff9f28);
          }
          lVar5 = FUN_00130700(&gp0xffff9f20);
          if (lVar5 == 0) {
            FUN_001280d0(*(undefined4 *)(iRam00660090 + 0x50),&gp0xffff9f20);
          }
          lVar5 = FUN_00130700(&gp0xffff9f18);
          if (lVar5 == 0) {
            FUN_001280d0(*(undefined4 *)(iRam00660090 + 0x50),&gp0xffff9f18);
          }
        }
      }
      uGpffff9e68 = 0x3f800000;
      (**(code **)(*piGpffff9e64 + 0x14))();
      FUN_00181c10(0x4b);
      iRam006600ac = FUN_00301700(lVar4,0x5f8d80,0x1004,0,1);
      FUN_00181c00();
      if (piRam0066008c != (int *)0x0) {
        if (piRam0066008c[0x26] == 0) {
          FUN_001dba60(0x65de20,0,0x8c);
        }
        else {
          FUN_0016aaa0(piRam0066008c[0x26],param_1);
        }
      }
      FUN_0013c200();
      if (*(int *)(iRam00660090 + 0x48) == 0) {
        *(undefined4 *)(iRam006600ac + 0x30) = 0;
      }
      else {
        FUN_001280d0(*(int *)(iRam00660090 + 0x48),&gp0xffff9f2c);
      }
      if (iRam006600ac == 0) {
        uVar2 = 0;
      }
      else {
        FUN_0013f6c0();
        if (*(int *)(iRam00660090 + 0x48) == 0) {
          *(undefined4 *)(iRam006600ac + 0x30) = 0x1000;
        }
        else {
          *(undefined4 *)(iRam006600ac + 0x30) = 0x1009;
        }
        fVar7 = (float)piRam0066008c[10];
        if (2.1474836e+09 <= fVar7) {
          fVar7 = fVar7 - 2.1474836e+09;
        }
        fVar8 = (float)piRam0066008c[0xb];
        if (2.1474836e+09 <= fVar8) {
          fVar8 = fVar8 - 2.1474836e+09;
        }
        fVar9 = (float)piRam0066008c[0xc];
        if (2.1474836e+09 <= fVar9) {
          fVar9 = fVar9 - 2.1474836e+09;
        }
        fVar10 = (float)piRam0066008c[0xd];
        if (2.1474836e+09 <= fVar10) {
          fVar10 = fVar10 - 2.1474836e+09;
        }
        FUN_00127370((int)fVar7 & 0xff,(int)fVar8 & 0xff,(int)fVar9 & 0xff,(int)fVar10 & 0xff);
        uRam006600b4 = 0;
        pcVar1 = (char *)piRam0066008c[0x23];
        if ((pcVar1 != (char *)0x0) && (*pcVar1 != '\0')) {
          iRam006600b0 = FUN_00301900(lVar4,pcVar1,0x201f,0);
        }
        if (iRam006600b0 != 0) {
          FUN_0012fee0(iRam006600b0,iRam006600ac + 0x2c);
        }
        FUN_0012f4e0(0x151c00,&gp0xffff9f14);
        FUN_0012f4e0(0x151c00,&gp0xffff9f2c);
        FUN_0012f4e0(0x151c00,&gp0xffff9f24);
        FUN_0012f4e0(0x151c00,&gp0xffff9f10);
        if ((piRam0066008c[0x1c] & 1U) != 0) {
          FUN_0021d790(iRam00660090 + 0x58);
        }
        uGpffffa768 = *(undefined4 *)(iRam00660090 + 0x54);
        FUN_0014f8c0();
        if ((iGpffff9db4 != 7) && (iGpffff9db4 != 6)) {
          FUN_0019dc80(piRam0066008c);
        }
        iVar6 = 0;
        lVar5 = FUN_00127100();
        if (0 < lVar5) {
          do {
            lVar5 = FUN_00181870(iVar6);
            if (lVar5 != 0) {
              FUN_001a6270(piRam0066008c[1],lVar5);
              FUN_001a62b0(piRam0066008c[2],lVar5);
            }
            iVar6 = iVar6 + 1;
            iVar3 = FUN_00127100();
          } while (iVar6 < iVar3);
        }
        iRam00660094 = param_1;
        FUN_00152740(lVar4);
        FUN_001dba60(0x65da20,0,0x400);
        uGpffff9f70 = 0;
        uGpffff8290 = 1;
        if (piRam0066008c[0x19] != 0) {
          FUN_00216630(uRam006600a0);
          FUN_00216460(uRam006600a0);
        }
        FUN_00516d90();
        uRam006600e8 = 0;
        if (*(int *)(iRam00660090 + 100) != 0) {
          FUN_00216630(uRam006600a0);
          FUN_00216460(uRam006600a0);
        }
        uVar2 = 1;
        uRam00660088 = 1;
        uRam006600f4 = 0;
      }
    }
  }
  else {
    uVar2 = 1;
  }
  return uVar2;
}

