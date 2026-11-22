
undefined4 FUN_0050d140(void)

{
  uint uVar1;
  int iVar2;
  undefined4 uVar3;
  long lVar4;
  int *piVar5;
  int iVar6;
  int iVar7;
  
  iVar2 = FUN_0012e930(piRam0064df54);
  if (iRam0064f250 == 0) {
    return 0xbf800000;
  }
  switch(uRam007bfcc8) {
  case 0:
  case 2:
    break;
  case 1:
    FUN_0044ac30(1);
    *(undefined4 *)(iRam0064f250 + 0x148) = 1;
    FUN_004404c0();
    if (-1 < (long)((ulong)*(byte *)(iRam0064f250 + 0xc) << 0x3a)) {
      FUN_002640d0(0,5);
    }
    *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0xdf | 0x20;
    *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0xdf;
    FUN_00478040();
    FUN_00480f00(0,0,0x3fc00000,0x40490fdb);
    lVar4 = FUN_00216060(*(undefined4 *)(iRam0064f298 + 0xb24),0x647840);
    if (lVar4 != 0) {
      FUN_00216630(*(undefined4 *)(iRam0064f298 + 0xb24),lVar4);
      FUN_00216460(*(undefined4 *)(iRam0064f298 + 0xb24));
    }
    FUN_0043fcd0(0x3dcccccd,0x3f4ccccd,0x3f4ccccd,0xc,5);
    FUN_00437bd0();
    uRam007bfcc8 = 3;
    if (-1 < (long)((ulong)*(byte *)(iRam0064f250 + 0xc) << 0x3a)) {
      FUN_002640d0(0,5);
    }
    *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0xdf | 0x20;
    *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0xdf;
    FUN_00478040();
    break;
  case 3:
switchD_0050d19c_caseD_3:
    goto LAB_0050d498;
  case 4:
    FUN_0044ada0();
    FUN_0044ac30(1);
    if (*(int *)(iRam0064f250 + 0x844) == 0) {
      FUN_0046b130();
      uVar3 = FUN_0012bf10(*(undefined4 *)
                            (*(int *)(*(int *)(*(int *)(iRam0064f250 + 0x100) * 4 + iRam0064f250 +
                                              0x2c) + 0xb18) + 0x114));
      *(undefined4 *)(iRam0064f250 + 0x840) = uVar3;
    }
    if (-1 < (long)((ulong)*(byte *)(iRam0064f250 + 0xc) << 0x3a)) {
      FUN_002640d0(0,5);
    }
    *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0xdf | 0x20;
    *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0xdf;
    FUN_004449b0(1);
    *(byte *)(iRam0064f250 + 0xc) = *(byte *)(iRam0064f250 + 0xc) & 0xbf | 0x40;
    FUN_004a0150(1);
    FUN_00478040();
    FUN_00480f00(0,0x3f800000,0x3fc00000,0x40490fdb);
    uRam0064df58 = 0x40000000;
    (**(code **)(*piRam0064df54 + 0x14))();
    *(byte *)(iRam0064f250 + 10) = *(byte *)(iRam0064f250 + 10) & 0xdf;
    lVar4 = FUN_00216060(*(undefined4 *)(iRam0064f298 + 0xb24),0x647840);
    if (lVar4 != 0) {
      FUN_00216630(*(undefined4 *)(iRam0064f298 + 0xb24),lVar4);
      FUN_00216460(*(undefined4 *)(iRam0064f298 + 0xb24));
      goto switchD_0050d19c_caseD_3;
    }
LAB_0050d498:
    uVar1 = *(uint *)(*(int *)(iRam0064f250 + 0x9ac) * 4 + 0x7bfccc);
    if (*(uint *)(iVar2 + 0xc) <= uVar1) {
      FUN_005103a0();
      *(undefined4 *)(*(int *)(iRam0064f250 + 0x9ac) * 4 + 0x7bfccc) = 0;
      FUN_0043fcd0(0x3dcccccd,0x3f4ccccd,0x3f4ccccd,0xd,5);
      FUN_00184520(10,1);
      uRam007bfc78 = uRam007bfc3c;
      uRam007bfc90 = uRam007bfc8c;
      uRam007bfc44 = 3;
      FUN_0012dda0(0xd018,0x20,0x511ea0,0,0);
      return 0xbf800000;
    }
    iVar7 = *(int *)(*(int *)(iVar2 + 8) + 8) + uVar1 * 0x54;
    FUN_0050f140(*(undefined4 *)(iRam0064f298 + 0xb24),iVar7,iVar7 + 0x18);
    FUN_0044ac30(1);
    uRam0064df58 = 0x41200000;
    (**(code **)(*piRam0064df54 + 0x14))();
    uRam007bfcc8 = 5;
    lVar4 = FUN_00216060(*(undefined4 *)(iRam0064f298 + 0xb24),0x647850);
    if (lVar4 != 0) {
      FUN_00216630(*(undefined4 *)(iRam0064f298 + 0xb24),lVar4);
      FUN_00216460(*(undefined4 *)(iRam0064f298 + 0xb24));
    }
    iVar7 = FUN_001818d0(0);
    *(undefined4 *)(iVar7 + 0x19c) = 0;
    FUN_0047a5f0(0,1);
    *(byte *)(iVar7 + 0x8c) = *(byte *)(iVar7 + 0x8c) & 0x7f | 0x80;
    FUN_00180d50(0,0x478ae0,1);
    FUN_0047b090();
    FUN_0046b130();
    uVar3 = FUN_0012bf10(*(undefined4 *)
                          (*(int *)(*(int *)(*(int *)(iRam0064f250 + 0x100) * 4 + iRam0064f250 +
                                            0x2c) + 0xb18) + 0x11c));
    *(undefined4 *)(iRam0064f250 + 0x840) = uVar3;
    uRam007bfcc8 = 5;
    FUN_00440420();
    FUN_0044ada0();
    if (*(int *)(*(int *)(*(int *)(iVar2 + 8) + 8) +
                 *(int *)(*(int *)(iRam0064f250 + 0x9ac) * 4 + 0x7bfccc) * 0x54 + 0x18) != 0) {
      FUN_0050fe50(0x3f000000,0x3f4ccccd);
    }
    FUN_0048b020();
    break;
  case 5:
    FUN_0050eac0(*(int *)(*(int *)(iVar2 + 8) + 8) +
                 *(int *)(*(int *)(iRam0064f250 + 0x9ac) * 4 + 0x7bfccc) * 0x54 + 0x18);
    break;
  case 6:
    uRam007bfcc8 = 8;
    FUN_004ecdd0();
    FUN_0012edc0(0xd024);
    FUN_0046a930(1);
    FUN_0046b130();
    uVar3 = FUN_0012bf10(*(undefined4 *)
                          (*(int *)(*(int *)(*(int *)(iRam0064f250 + 0x100) * 4 + iRam0064f250 +
                                            0x2c) + 0xb18) + 0x114));
    *(undefined4 *)(iRam0064f250 + 0x840) = uVar3;
    iVar7 = *(int *)(iRam0064f250 + 0xa04);
    if (iVar7 == 0) {
      iVar7 = 0;
    }
    else if (*(int *)(iVar7 + 4) != *(int *)(iRam0064f250 + 0xa08)) {
      iVar7 = 0;
    }
    if (iVar7 != 0) {
      FUN_0016fde0();
    }
    iVar7 = 0;
    piVar5 = (int *)(iRam0064f250 + 0xa5c);
    do {
      iVar6 = *piVar5;
      if (iVar6 == 0) {
        iVar6 = 0;
      }
      else if (*(int *)(iVar6 + 4) != piVar5[1]) {
        iVar6 = 0;
      }
      if (iVar6 != 0) {
        *(byte *)(iVar6 + 0xc) = *(byte *)(iVar6 + 0xc) & 0xf7 | 8;
      }
      iVar7 = iVar7 + 1;
      piVar5 = piVar5 + 2;
    } while (iVar7 < 5);
    iVar7 = *(int *)(iRam0064f250 + 0x9fc);
    if (iVar7 == 0) {
      iVar7 = 0;
    }
    else if (*(int *)(iVar7 + 4) != *(int *)(iRam0064f250 + 0xa00)) {
      iVar7 = 0;
    }
    if (iVar7 != 0) {
      FUN_0016fde0();
    }
    iVar7 = 0;
    piVar5 = (int *)(iRam0064f250 + 0xa0c);
    do {
      iVar6 = *piVar5;
      if (iVar6 == 0) {
        iVar6 = 0;
      }
      else if (*(int *)(iVar6 + 4) != piVar5[1]) {
        iVar6 = 0;
      }
      if (iVar6 != 0) {
        *(byte *)(iVar6 + 0xc) = *(byte *)(iVar6 + 0xc) & 0xf7 | 8;
      }
      iVar7 = iVar7 + 1;
      piVar5 = piVar5 + 2;
    } while (iVar7 < 5);
    FUN_004403d0();
    FUN_0044ac30(1);
    FUN_004404c0();
    if (iRam007c25c8 == 0) {
      FUN_0050de80(*(undefined4 *)(iRam0064f298 + 0xb24),
                   *(int *)(*(int *)(iVar2 + 8) + 8) +
                   *(int *)(*(int *)(iRam0064f250 + 0x9ac) * 4 + 0x7bfccc) * 0x54 + 0x30);
      if (*(uint *)(*(int *)(iVar2 + 8) + 4) <=
          *(uint *)(*(int *)(iRam0064f250 + 0x9ac) * 4 + 0x7bfccc)) {
        uRam007bfcc8 = 8;
      }
      *(undefined4 *)(iVar2 + 0x10) = 0;
    }
    else {
      FUN_0050db60(*(undefined4 *)(iRam0064f298 + 0xb24),
                   *(int *)(*(int *)(iVar2 + 8) + 8) +
                   *(int *)(*(int *)(iRam0064f250 + 0x9ac) * 4 + 0x7bfccc) * 0x54 + 0x3c);
      *(undefined4 *)(iVar2 + 0x10) = 0;
    }
    break;
  case 7:
    uVar1 = *(uint *)(*(int *)(iRam0064f250 + 0x9ac) * 4 + 0x7bfccc);
    if ((uVar1 < *(uint *)(*(int *)(iVar2 + 8) + 4)) || (*(uint *)(iVar2 + 0xc) <= uVar1 + 1)) {
      uRam007bfcc8 = 4;
      if (iRam007c25c8 == 0) {
        uRam007bfcc8 = 3;
        goto LAB_0050da50;
      }
      iVar2 = *(int *)(iRam0064f250 + 0x9ac);
    }
    else {
      uRam007bfcc8 = 8;
LAB_0050da50:
      iVar2 = *(int *)(iRam0064f250 + 0x9ac);
    }
    piVar5 = (int *)(iVar2 * 4 + 0x7bfccc);
    *piVar5 = *piVar5 + 1;
    break;
  case 8:
    FUN_005103a0();
    lVar4 = FUN_0043fcd0(0x3dcccccd,0x3f4ccccd,0x3f4ccccd,0xe,7);
    if (lVar4 == 2) {
      FUN_00184520(10,1);
      uRam007bfc78 = uRam007bfc3c;
      uRam007bfc90 = uRam007bfc8c;
      uRam007bfc44 = 3;
      FUN_0012dda0(0xd018,0x20,0x511ea0,0,0);
      return 0xbf800000;
    }
    uRam007bfcc8 = 4;
    if (iRam007c25c8 == 0) {
      uRam007bfcc8 = 3;
    }
    break;
  default:
    goto switchD_0050d19c_default;
  }
switchD_0050d19c_default:
  return 0x3f800000;
}

