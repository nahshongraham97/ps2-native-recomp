
void FUN_00245dd0(long param_1)

{
  int iVar1;
  undefined1 *puVar2;
  long lVar3;
  undefined8 uVar4;
  undefined4 *puVar5;
  int iVar6;
  undefined1 *puVar7;
  int iVar8;
  int *piVar9;
  int *piVar10;
  undefined4 *puVar11;
  
  lVar3 = FUN_00281830();
  if (param_1 == 1) {
    piVar10 = (int *)0x64e910;
    piVar9 = (int *)0x64e90c;
    puVar11 = (undefined4 *)0x64e914;
    iVar8 = 0x660174;
    puVar7 = (undefined1 *)0x6aba40;
  }
  else {
    if (param_1 != 0) {
      return;
    }
    piVar10 = (int *)0x64e91c;
    piVar9 = (int *)0x64e918;
    puVar11 = (undefined4 *)0x64e920;
    iVar8 = 0x66012c;
    puVar7 = (undefined1 *)0x6af670;
  }
  if ((((piVar10 != (int *)0x0) && (piVar9 != (int *)0x0)) && (iVar8 != 0)) &&
     (puVar7 != (undefined1 *)0x0)) {
    iVar6 = *piVar9;
    iVar1 = *piVar10;
    if (((iVar1 != -1 && iVar6 != -1) && (-1 < iVar1)) &&
       ((iVar1 < 2 && ((-1 < iVar6 && (iVar6 < 8)))))) {
      *(undefined4 *)(iVar6 * 4 + iVar1 * 0x1e218 + 0x660658) = 0;
    }
    if (lVar3 != 0) {
      *(undefined4 *)lVar3 = 0;
    }
    FUN_001dba60(puVar7,0,0x3c30);
    *puVar7 = 1;
    uVar4 = FUN_00263e40(0);
    FUN_001dec58(puVar7 + 8,uVar4);
    iVar6 = 0;
    puVar5 = &DAT_005be100;
    puVar2 = puVar7;
    do {
      iVar6 = iVar6 + 8;
      *(undefined4 *)(puVar2 + 0x7c) = *puVar5;
      *(undefined4 *)(puVar2 + 0xbc) = *puVar5;
      *(undefined4 *)(puVar2 + 0x80) = puVar5[3];
      *(undefined4 *)(puVar2 + 0xc0) = puVar5[3];
      *(undefined4 *)(puVar2 + 0x84) = puVar5[6];
      *(undefined4 *)(puVar2 + 0xc4) = puVar5[6];
      *(undefined4 *)(puVar2 + 0x88) = puVar5[9];
      *(undefined4 *)(puVar2 + 200) = puVar5[9];
      *(undefined4 *)(puVar2 + 0x8c) = puVar5[0xc];
      *(undefined4 *)(puVar2 + 0xcc) = puVar5[0xc];
      *(undefined4 *)(puVar2 + 0x90) = puVar5[0xf];
      *(undefined4 *)(puVar2 + 0xd0) = puVar5[0xf];
      *(undefined4 *)(puVar2 + 0x94) = puVar5[0x12];
      *(undefined4 *)(puVar2 + 0xd4) = puVar5[0x12];
      *(undefined4 *)(puVar2 + 0x98) = puVar5[0x15];
      *(undefined4 *)(puVar2 + 0xd8) = puVar5[0x15];
      puVar5 = puVar5 + 0x18;
      puVar2 = puVar2 + 0x20;
    } while (iVar6 < 0x10);
    FUN_00514d30(puVar7);
    FUN_004f3c80(puVar7 + 0x930);
    FUN_001dba60(puVar7 + 0x120,0xff,0xa0);
    FUN_001dba60(puVar7 + 0x1c0,0xff,0x78);
    *puVar11 = 0;
    *piVar10 = -1;
    *piVar9 = -1;
    FUN_002817b0(iVar8);
    (&DAT_0064c720)[(int)param_1] = 8;
  }
  return;
}

