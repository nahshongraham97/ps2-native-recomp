
void FUN_004ee960(int param_1,undefined8 param_2)

{
  undefined4 uVar1;
  int iVar2;
  long lVar3;
  undefined8 uVar4;
  int *piVar5;
  char *pcVar6;
  int iVar7;
  int iVar8;
  int iVar9;
  int iStack_30;
  undefined1 auStack_4 [4];
  
  iVar2 = FUN_00245bd0(*(undefined4 *)(param_1 + 4),auStack_4);
  uVar1 = *(undefined4 *)(*(int *)(param_1 + 0x30) + 0x9d4);
  if (iRam0064dea4 == 10) {
    lVar3 = FUN_0012ed10(0x2001);
    if (lVar3 == 0) {
      lVar3 = 0;
      goto LAB_004ee9f0;
    }
    lVar3 = FUN_00130860((int)lVar3 + 0xe8);
    if ((lVar3 != 0) && (lVar3 != 0)) goto LAB_004ee9f0;
  }
  lVar3 = 0;
LAB_004ee9f0:
  if (lVar3 != 0) {
    uVar1 = *(undefined4 *)((int)lVar3 + 0x7b4);
  }
  lVar3 = FUN_00215df0(0x646e78,0,uVar1);
  if ((iVar2 != 0) && (lVar3 != 0)) {
    iVar9 = 0;
    iStack_30 = 0;
    do {
      uVar1 = *(undefined4 *)(*(int *)(*(int *)(param_1 + 0x30) + iStack_30 + 0x440) + 8);
      iVar7 = iVar2 + 0x200;
      if (iVar9 == 0) {
        pcVar6 = &DAT_005f1260;
        piVar5 = (int *)lVar3;
      }
      else {
        pcVar6 = &DAT_005f1330;
        piVar5 = (int *)lVar3 + 0x28;
      }
      iVar8 = 0;
      do {
        if (((pcVar6[1] != -1) && (*pcVar6 < '\x1a')) && (*(short *)(iVar7 + 2) != 0)) {
          uVar4 = FUN_00215d90(uVar1,*piVar5 + 1);
          FUN_004f1180(*(undefined4 *)(pcVar6 + 8),0x7bd170);
          FUN_0026c440(param_2,uVar4,iVar9,0x7bd170,0);
        }
        iVar8 = iVar8 + 1;
        iVar7 = iVar7 + 4;
        pcVar6 = pcVar6 + 0xc;
        piVar5 = piVar5 + 2;
      } while (iVar8 < 0x14);
      iVar9 = iVar9 + 1;
      iStack_30 = iStack_30 + 4;
      iVar2 = iVar2 + 0x6c;
    } while (iVar9 < 2);
  }
  return;
}

