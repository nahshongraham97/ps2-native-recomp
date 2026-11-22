
void FUN_00243570(int param_1)

{
  bool bVar1;
  int iVar2;
  int *piVar3;
  int iVar4;
  char *pcVar5;
  int iVar6;
  char *pcVar7;
  int *piVar8;
  int *piVar9;
  int *piVar10;
  
  iVar2 = iRam006b329c;
  piVar3 = (int *)(param_1 * 0x1e218 + 0x660640);
  if (*piVar3 == 0) {
    if (iGpffffa830 == 1) {
      pcVar5 = (char *)0x6af683;
      iVar4 = 0;
      iVar6 = iGpffffa828 * 0x3c30 + iGpffffa82c * 0x1e218;
      pcVar7 = (char *)(iVar6 + 0x6606e3);
      do {
        if (*pcVar5 != *pcVar7) {
          bVar1 = false;
          goto LAB_00243638;
        }
        iVar4 = iVar4 + 1;
        pcVar5 = pcVar5 + 1;
        pcVar7 = pcVar7 + 1;
      } while (iVar4 < 6);
      bVar1 = true;
LAB_00243638:
      if (bVar1) {
        pcVar5 = (char *)(iVar6 + 0x6606d8);
        pcVar7 = (char *)0x6af678;
        iVar4 = 0;
        do {
          if (*pcVar7 != *pcVar5) {
            bVar1 = false;
            goto LAB_00243684;
          }
          iVar4 = iVar4 + 1;
          pcVar7 = pcVar7 + 1;
          pcVar5 = pcVar5 + 1;
        } while (iVar4 < 0xb);
        bVar1 = true;
LAB_00243684:
        if (bVar1) {
          bVar1 = iRam006b329c == *(int *)(iVar6 + 0x6642fc);
        }
        else {
          bVar1 = false;
        }
      }
      else {
        bVar1 = false;
      }
      if (bVar1) {
        *(undefined4 *)(iGpffffa828 * 4 + iGpffffa82c * 0x1e218 + 0x660658) = 1;
      }
      else {
        iVar4 = 0;
        piVar8 = piVar3;
        piVar9 = piVar3;
        do {
          if (piVar9[0xf2e] == 1) {
            pcVar7 = (char *)((int)piVar9 + 0xa3);
            iVar6 = 0;
            pcVar5 = (char *)0x6af683;
            do {
              if (*pcVar5 != *pcVar7) {
                bVar1 = false;
                goto LAB_00243720;
              }
              iVar6 = iVar6 + 1;
              pcVar5 = pcVar5 + 1;
              pcVar7 = pcVar7 + 1;
            } while (iVar6 < 6);
            bVar1 = true;
LAB_00243720:
            if (bVar1) {
              piVar10 = piVar9 + 0x26;
              iVar6 = 0;
              pcVar5 = (char *)0x6af678;
              do {
                if (*pcVar5 != (char)*piVar10) {
                  bVar1 = false;
                  goto LAB_00243770;
                }
                iVar6 = iVar6 + 1;
                pcVar5 = pcVar5 + 1;
                piVar10 = (int *)((int)piVar10 + 1);
              } while (iVar6 < 0xb);
              bVar1 = true;
LAB_00243770:
              if (bVar1) {
                bVar1 = iVar2 == piVar9[0xf2f];
              }
              else {
                bVar1 = false;
              }
            }
            else {
              bVar1 = false;
            }
            if (bVar1) {
              piVar8[6] = 1;
              iGpffffa828 = iVar4;
              iGpffffa82c = param_1;
            }
          }
          iVar4 = iVar4 + 1;
          piVar9 = piVar9 + 0xf0c;
          piVar8 = piVar8 + 1;
        } while (iVar4 < 8);
      }
    }
    iVar2 = iRam006af66c;
    if (iGpffffa824 == 1) {
      pcVar5 = (char *)0x6aba53;
      iVar4 = 0;
      iVar6 = iGpffffa81c * 0x3c30 + iGpffffa820 * 0x1e218;
      pcVar7 = (char *)(iVar6 + 0x6606e3);
      do {
        if (*pcVar5 != *pcVar7) {
          bVar1 = false;
          goto LAB_00243874;
        }
        iVar4 = iVar4 + 1;
        pcVar5 = pcVar5 + 1;
        pcVar7 = pcVar7 + 1;
      } while (iVar4 < 6);
      bVar1 = true;
LAB_00243874:
      if (bVar1) {
        pcVar5 = (char *)(iVar6 + 0x6606d8);
        pcVar7 = (char *)0x6aba48;
        iVar4 = 0;
        do {
          if (*pcVar7 != *pcVar5) {
            bVar1 = false;
            goto LAB_002438c0;
          }
          iVar4 = iVar4 + 1;
          pcVar7 = pcVar7 + 1;
          pcVar5 = pcVar5 + 1;
        } while (iVar4 < 0xb);
        bVar1 = true;
LAB_002438c0:
        if (bVar1) {
          bVar1 = iRam006af66c == *(int *)(iVar6 + 0x6642fc);
        }
        else {
          bVar1 = false;
        }
      }
      else {
        bVar1 = false;
      }
      if (bVar1) {
        *(undefined4 *)(iGpffffa81c * 4 + iGpffffa820 * 0x1e218 + 0x660658) = 1;
      }
      else {
        iVar4 = 0;
        piVar8 = piVar3;
        do {
          if (piVar3[0xf2e] == 1) {
            pcVar7 = (char *)((int)piVar3 + 0xa3);
            iVar6 = 0;
            pcVar5 = (char *)0x6aba53;
            do {
              if (*pcVar5 != *pcVar7) {
                bVar1 = false;
                goto LAB_00243958;
              }
              iVar6 = iVar6 + 1;
              pcVar5 = pcVar5 + 1;
              pcVar7 = pcVar7 + 1;
            } while (iVar6 < 6);
            bVar1 = true;
LAB_00243958:
            if (bVar1) {
              piVar9 = piVar3 + 0x26;
              iVar6 = 0;
              pcVar5 = (char *)0x6aba48;
              do {
                if (*pcVar5 != (char)*piVar9) {
                  bVar1 = false;
                  goto LAB_002439a8;
                }
                iVar6 = iVar6 + 1;
                pcVar5 = pcVar5 + 1;
                piVar9 = (int *)((int)piVar9 + 1);
              } while (iVar6 < 0xb);
              bVar1 = true;
LAB_002439a8:
              if (bVar1) {
                bVar1 = iVar2 == piVar3[0xf2f];
              }
              else {
                bVar1 = false;
              }
            }
            else {
              bVar1 = false;
            }
            if (bVar1) {
              piVar8[6] = 1;
              iGpffffa81c = iVar4;
              iGpffffa820 = param_1;
            }
          }
          iVar4 = iVar4 + 1;
          piVar3 = piVar3 + 0xf0c;
          piVar8 = piVar8 + 1;
        } while (iVar4 < 8);
      }
    }
  }
  return;
}

