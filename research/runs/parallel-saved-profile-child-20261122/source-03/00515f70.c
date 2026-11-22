
undefined4 FUN_00515f70(undefined8 param_1)

{
  int *piVar1;
  bool bVar2;
  undefined4 uVar3;
  long lVar4;
  int iVar5;
  int iVar6;
  uint uVar7;
  int iVar8;
  int *piVar9;
  int iVar10;
  undefined4 uStack_28;
  undefined4 uStack_24;
  undefined4 uStack_20;
  undefined1 auStack_1c [4];
  undefined1 auStack_18 [4];
  undefined1 auStack_14 [4];
  undefined4 uStack_10;
  byte *pbStack_c;
  undefined4 uStack_8;
  int iStack_4;
  
  uVar7 = 0;
  iVar5 = (int)param_1;
  iVar6 = 0x7bd230;
  do {
    if (*(uint *)(iVar6 + 0xac) == (*(uint *)(iVar5 + 0xc) | 0x10000000)) {
      iVar6 = uVar7 * 0xa28 + 0x7bd2dc;
      goto LAB_00515ff0;
    }
    uVar7 = uVar7 + 1;
    iVar6 = iVar6 + 0xa28;
  } while (uVar7 < 4);
  iVar6 = 0;
LAB_00515ff0:
  if (iVar6 == 0) {
    uVar3 = 0;
  }
  else {
    uVar7 = 0;
    iVar10 = iVar6;
    if (*(int *)(iVar6 + 4) != 0) {
      do {
        lVar4 = FUN_004a9d10(param_1,*(undefined4 *)(iVar10 + 8),auStack_1c);
        if (lVar4 != 0) {
          iVar8 = (int)lVar4;
          FUN_0045d2b0(iVar8 + 8,&uStack_28,&uStack_10);
          *(undefined4 *)(iVar8 + 0xc) = uStack_28;
          *(undefined4 *)(iVar8 + 0x10) = uStack_24;
          *(undefined4 *)(iVar8 + 0x14) = uStack_20;
          *(undefined4 *)(iVar8 + 0x18) = uStack_10;
          lVar4 = FUN_0051a270(uStack_28,uStack_24,uStack_20,uStack_10,*(undefined4 *)(iVar8 + 4),
                               lVar4,1);
          pbStack_c = (byte *)lVar4;
          if (lVar4 != 0) {
            lVar4 = FUN_004a9be0(param_1,*(undefined4 *)(iVar8 + 4),lVar4,&iStack_4,&uStack_8);
            if ((*(char *)(iVar10 + 0xd) != '\0') && (lVar4 != 0)) {
              FUN_0051ea90(iStack_4,0,uStack_8,*(char *)(iVar10 + 0xd),&pbStack_c,auStack_14,
                           auStack_18);
            }
            *(uint *)(pbStack_c + 0x14) = (uint)*(byte *)(iVar10 + 0xc);
            *(undefined4 *)pbStack_c = *(undefined4 *)(iVar10 + 0x10);
            if (((*pbStack_c & 1) != 0) && (lVar4 != 0)) {
              uVar3 = FUN_00451b90(uRam0064f3a4,1,0);
              *(undefined4 *)(pbStack_c + 0xc) = uVar3;
            }
          }
        }
        uVar7 = uVar7 + 1;
        iVar10 = iVar10 + 0xc;
      } while (uVar7 < *(uint *)(iVar6 + 4));
    }
    if ((iVar5 != -0xe7c) && (piVar9 = *(int **)(iVar5 + 0xe7c), piVar9 != (int *)0x0)) {
      iVar6 = *piVar9;
      do {
        if (piVar9[4] == *(int *)(iVar6 + 4)) {
          iVar10 = *(int *)(*(int *)(iVar6 + 0x3c) + 0x6c);
          iStack_4 = iVar6;
          if (iVar10 != 0) {
            bVar2 = false;
            uVar7 = 0;
            do {
              if ((*(int *)(iVar6 + 0x2c) != 0) && (*(int *)(*(int *)(iVar6 + 0x2c) + 0xc) != 0)) {
                bVar2 = true;
                break;
              }
              uVar7 = uVar7 + 1;
              iVar6 = iVar6 + 4;
            } while (uVar7 < 4);
            if (!bVar2) {
              FUN_0019d190(*(undefined4 *)(iVar5 + 0xf9c),iVar10,1);
            }
          }
          piVar1 = (int *)piVar9[1];
        }
        else {
          piVar1 = (int *)piVar9[1];
          piVar9[4] = 1;
          FUN_0012fa00(piVar9);
        }
        if (piVar1 == (int *)0x0) break;
        iVar6 = *piVar1;
        piVar9 = piVar1;
      } while( true );
    }
    uVar3 = 1;
  }
  return uVar3;
}

