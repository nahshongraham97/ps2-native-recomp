
undefined4 FUN_002451f0(void)

{
  undefined4 uVar1;
  undefined4 uVar2;
  undefined4 uVar3;
  undefined8 uVar4;
  int iVar5;
  undefined4 *puVar6;
  int iVar7;
  
  if (iRam0064e914 == 0) {
    FUN_001db8a8(0x6aba40,0x6af670,0x3c30);
    uRam0064e90c = uRam0064e918;
    iRam0064e914 = iRam0064e920;
    uRam0064e910 = uRam0064e91c;
    DAT_0064c724 = DAT_0064c720;
    FUN_001dba60(0x6af670,0,0x3c30);
    uRam006af670 = 1;
    uVar4 = FUN_00263e40(0);
    FUN_001dec58(0x6af678,uVar4);
    iVar7 = 0;
    puVar6 = &DAT_005be100;
    iVar5 = 0x6af670;
    do {
      uVar3 = *puVar6;
      iVar7 = iVar7 + 8;
      uVar1 = puVar6[3];
      uVar2 = puVar6[6];
      *(undefined4 *)(iVar5 + 0x7c) = uVar3;
      *(undefined4 *)(iVar5 + 0xbc) = uVar3;
      *(undefined4 *)(iVar5 + 0x80) = uVar1;
      *(undefined4 *)(iVar5 + 0xc0) = uVar1;
      *(undefined4 *)(iVar5 + 0x84) = uVar2;
      *(undefined4 *)(iVar5 + 0xc4) = uVar2;
      uVar3 = puVar6[9];
      uVar1 = puVar6[0xc];
      uVar2 = puVar6[0xf];
      *(undefined4 *)(iVar5 + 0x88) = uVar3;
      *(undefined4 *)(iVar5 + 200) = uVar3;
      *(undefined4 *)(iVar5 + 0x8c) = uVar1;
      *(undefined4 *)(iVar5 + 0xcc) = uVar1;
      *(undefined4 *)(iVar5 + 0x90) = uVar2;
      *(undefined4 *)(iVar5 + 0xd0) = uVar2;
      uVar3 = puVar6[0x12];
      uVar1 = puVar6[0x15];
      *(undefined4 *)(iVar5 + 0x94) = uVar3;
      *(undefined4 *)(iVar5 + 0xd4) = uVar3;
      puVar6 = puVar6 + 0x18;
      *(undefined4 *)(iVar5 + 0x98) = uVar1;
      *(undefined4 *)(iVar5 + 0xd8) = uVar1;
      iVar5 = iVar5 + 0x20;
    } while (iVar7 < 0x10);
    FUN_00514d30(0x6af670);
    FUN_004f3c80(0x6affa0);
    FUN_001dba60(0x6af790,0xff,0xa0);
    FUN_001dba60(0x6af830,0xff,0x78);
    iRam0064e920 = 0;
    uRam0064e91c = 0xffffffff;
    uVar3 = 1;
    uRam0064e918 = 0xffffffff;
    DAT_0064c720 = 0;
  }
  else {
    uVar3 = 0;
  }
  return uVar3;
}

