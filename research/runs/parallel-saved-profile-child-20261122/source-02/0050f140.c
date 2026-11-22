
undefined4 FUN_0050f140(undefined8 param_1,uint *param_2,int param_3)

{
  byte bVar1;
  undefined4 uVar2;
  long lVar3;
  long lVar4;
  uint uVar5;
  int iVar6;
  undefined4 *puVar7;
  undefined4 *puVar8;
  int *piVar9;
  int iVar10;
  uint uVar11;
  int iVar12;
  undefined4 auStack_70 [27];
  int iStack_4;
  
  puVar8 = &DAT_005f24e0;
  puVar7 = auStack_70;
  iVar6 = 0xc;
  do {
    iVar6 = iVar6 + -1;
    uVar2 = puVar8[1];
    *puVar7 = *puVar8;
    puVar7[1] = uVar2;
    puVar8 = puVar8 + 2;
    puVar7 = puVar7 + 2;
  } while (0 < iVar6);
  uVar11 = param_2[5];
  iVar6 = 1;
  if ((uVar11 & 0x400) == 0) {
    if ((uVar11 & 0x800) == 0) {
      if ((uVar11 & 0x1000) == 0) {
        if ((uVar11 & 0x2000) == 0) {
          if ((uVar11 & 0x4000) == 0) {
            if ((uVar11 & 0x8000) == 0) {
              if ((uVar11 & 0x10000) == 0) {
                *(byte *)(iRam0064f250 + 0xd) = *(byte *)(iRam0064f250 + 0xd) & 0xbf | 0x40;
              }
              else {
                FUN_0050f9a0(6);
              }
            }
            else {
              FUN_0050f9a0(7);
            }
          }
          else {
            FUN_0050f9a0(5);
          }
        }
        else {
          FUN_0050f9a0(0);
        }
      }
      else {
        FUN_0050f9a0(4);
      }
    }
    else {
      FUN_0050f9a0(3);
    }
  }
  else {
    FUN_0050f9a0(1);
  }
  FUN_0043fcd0(0x3dcccccd,0x3f333333,0x3f4ccccd,param_2[4],5);
  *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0xbf;
  FUN_0044ac30(1);
  if (*param_2 == 2) {
    iVar10 = 0;
    piVar9 = (int *)(iRam0064f250 + 0xa5c);
    do {
      iVar12 = *piVar9;
      if (iVar12 == 0) {
        iVar12 = 0;
      }
      else if (*(int *)(iVar12 + 4) != piVar9[1]) {
        iVar12 = 0;
      }
      if (iVar12 != 0) {
        *(byte *)(iVar12 + 0xc) = *(byte *)(iVar12 + 0xc) & 0xf7 | 8;
      }
      iVar10 = iVar10 + 1;
      piVar9 = piVar9 + 2;
    } while (iVar10 < 5);
    iVar10 = *(int *)(iRam0064f250 + 0x9fc);
    if (iVar10 == 0) {
      iVar10 = 0;
    }
    else if (*(int *)(iVar10 + 4) != *(int *)(iRam0064f250 + 0xa00)) {
      iVar10 = 0;
    }
    if (iVar10 != 0) {
      FUN_0016fde0();
    }
    iVar10 = 0;
    piVar9 = (int *)(iRam0064f250 + 0xa0c);
    do {
      iVar12 = *piVar9;
      if (iVar12 == 0) {
        iVar12 = 0;
      }
      else if (*(int *)(iVar12 + 4) != piVar9[1]) {
        iVar12 = 0;
      }
      if (iVar12 != 0) {
        *(byte *)(iVar12 + 0xc) = *(byte *)(iVar12 + 0xc) & 0xf7 | 8;
      }
      iVar10 = iVar10 + 1;
      piVar9 = piVar9 + 2;
    } while (iVar10 < 5);
    iStack_4 = 0;
    lVar3 = FUN_0012dda0(0x9030,0x20,0x510e80,0xc,&iStack_4);
    if ((lVar3 != 0) && (iStack_4 != 0)) {
      *(undefined4 *)(iStack_4 + 8) = 0;
    }
    uRam0064df58 = 0x42480000;
    (**(code **)(*piRam0064df54 + 0x14))();
    *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0xbf | 0x40;
    FUN_004a0150(1);
    FUN_004449b0(1);
    FUN_00184500(0x14,1);
    if (*(int *)(iRam0064f250 + 0x9ac) == 4) {
      FUN_00511350(9,param_2[1],0,0,*(undefined4 *)(*(int *)(iRam0064f250 + 0x30) + 0xc),1,0x40,
                   param_2[2]);
    }
    else if (*(int *)(iRam0064f250 + 0x9ac) == 6) {
      FUN_00511350(0x23,param_2[1],0,0,*(undefined4 *)(*(int *)(iRam0064f250 + 0x30) + 0xc),1,0x40,
                   param_2[2]);
    }
    else {
      FUN_00511350(0x23,param_2[1],0,0,*(undefined4 *)(*(int *)(iRam0064f250 + 0x30) + 0xc),0,0x40,
                   param_2[2]);
    }
  }
  bVar1 = *(byte *)(iRam0064f250 + 0xd);
  while (-1 < (long)((ulong)bVar1 << 0x39)) {
    uRam0064df58 = 0x3f800000;
    (**(code **)(*piRam0064df54 + 0x14))();
    bVar1 = *(byte *)(iRam0064f250 + 0xd);
  }
  uVar11 = 0;
  if (param_2[2] != 0) {
    uVar5 = *param_2;
    iVar6 = 1;
    while( true ) {
      if ((uVar5 < 2) &&
         (uVar5 = uVar11 & 7,
         lVar3 = FUN_00485d90(auStack_70[uVar5 * 3],auStack_70[uVar5 * 3 + 1],
                              auStack_70[uVar5 * 3 + 2],0,1,param_2[1]), lVar3 != 0)) {
        lVar4 = FUN_0049f020(lVar3);
        if (lVar4 != 0) {
          FUN_004a0740(lVar4,0x50fda0);
        }
        iVar12 = (int)lVar3;
        iVar10 = *(int *)(iVar12 + 0xc);
        if (iVar10 == 0) {
          iVar10 = 0;
        }
        else if (*(int *)(iVar10 + 4) != *(int *)(iVar12 + 0x10)) {
          iVar10 = 0;
        }
        FUN_0045c800(0x6478e8,iVar10,6,0,0,1,2);
        if (*param_2 == 0) {
          *(byte *)(iVar12 + 0x7c) = *(byte *)(iVar12 + 0x7c) & 0xbf | 0x40;
        }
        if ((param_2[5] & 0x10) != 0) {
          iVar6 = 0;
          *(byte *)(iVar12 + 0x7c) = *(byte *)(iVar12 + 0x7c) & 0xbf;
          *(byte *)(iVar12 + 0x7d) = *(byte *)(iVar12 + 0x7d) & 0xfe | 1;
          *(undefined4 *)(iVar12 + 0x1e0) = 0x3d4ccccd;
        }
        *(byte *)(iVar12 + 0x7c) = *(byte *)(iVar12 + 0x7c) & 0x7f | 0x80;
      }
      uVar11 = uVar11 + 1;
      if (param_2[2] <= uVar11) break;
      uVar5 = *param_2;
    }
  }
  FUN_00466b80(0xc47a0000);
  FUN_00466b10(0xc47a0000);
  *(undefined4 *)(iRam0064f250 + 0x9dc) = 0;
  *(undefined4 *)(iRam0064f250 + 0x9d8) = 0;
  *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0x7f;
  *(undefined4 *)(iRam0064f250 + 0x9cc) = 0;
  *(undefined4 *)(iRam0064f250 + 0x9c8) = 0;
  *(undefined4 *)(iRam0064f250 + 0x9d0) = 0;
  *(byte *)(iRam0064f250 + 0xd) = *(byte *)(iRam0064f250 + 0xd) & 0xfe;
  *(byte *)(iRam0064f250 + 0xd) = *(byte *)(iRam0064f250 + 0xd) & 0xfd;
  *(byte *)(iRam0064f250 + 0xd) = *(byte *)(iRam0064f250 + 0xd) & 0xfb;
  *(undefined4 *)(iRam0064f250 + 0x9d4) = 0;
  if ((param_2[5] & 1) == 0) {
    uVar11 = param_2[5];
  }
  else {
    *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0x7f | 0x80;
    uVar11 = param_2[5];
  }
  if ((uVar11 & 2) == 0) {
    uVar11 = param_2[5];
  }
  else {
    *(byte *)(iRam0064f250 + 0xd) = *(byte *)(iRam0064f250 + 0xd) & 0xfe | 1;
    uVar11 = param_2[5];
  }
  if ((uVar11 & 8) == 0) {
    iVar10 = *(int *)(param_3 + 4);
  }
  else {
    *(byte *)(iRam0064f250 + 0xd) = *(byte *)(iRam0064f250 + 0xd) & 0xfb | 4;
    iVar10 = *(int *)(param_3 + 4);
  }
  if (iVar10 == 0) {
    *(undefined4 *)(iRam0064f250 + 0x9d4) = *(undefined4 *)(param_3 + 0xc);
    *(undefined4 *)(iRam0064f250 + 0x9d8) = *(undefined4 *)(param_3 + 8);
    *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0xbf | (byte)(iVar6 << 6);
    FUN_00510710(*(undefined4 *)(param_3 + 0x14));
    FUN_005103a0();
    uVar11 = *param_2;
  }
  else {
    uVar11 = *param_2;
  }
  if (((uVar11 == 0) || (uVar11 == 1)) || (uVar11 == 3)) {
    lVar3 = FUN_00216060(param_1,0x647900);
    *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0xbf | (byte)(iVar6 << 6);
    if (lVar3 != 0) {
      FUN_00216630(param_1,lVar3);
      FUN_00216460(param_1);
      uRam0064df58 = 0x42700000;
      (**(code **)(*piRam0064df54 + 0x14))();
    }
    if ((param_2[5] & 0x40) != 0) {
      FUN_0050fbe0(0x3a03126f,1);
    }
    uVar11 = *param_2;
  }
  else {
    uVar11 = *param_2;
  }
  if (uVar11 == 3) {
    FUN_00216630(param_1,param_2[3]);
    FUN_00216460(param_1);
    iVar10 = *(int *)(param_3 + 4);
  }
  else {
    iVar10 = *(int *)(param_3 + 4);
  }
  if (iVar10 == 3) {
    *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0xbf | (byte)(iVar6 << 6);
    *(byte *)(iRam0064f250 + 0xd) = *(byte *)(iRam0064f250 + 0xd) & 0xfd | 2;
    FUN_00215430(*(undefined4 *)
                  (*(int *)(*(int *)(iRam0064f250 + 0x100) * 4 + iRam0064f250 + 0x2c) + 0xb24),
                 *(undefined4 *)(param_3 + 0x10),0);
  }
  FUN_0044ac30(0);
  return 0;
}

