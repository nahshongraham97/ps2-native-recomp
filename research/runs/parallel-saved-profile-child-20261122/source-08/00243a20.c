
undefined4 FUN_00243a20(long param_1)

{
  bool bVar1;
  long lVar2;
  char *pcVar3;
  int iVar4;
  int iVar5;
  char *pcVar6;
  int *piVar7;
  int *piVar8;
  int iVar9;
  int iVar10;
  int iStack_8;
  int iStack_4;
  
  iStack_4 = 0;
  iStack_8 = 0;
  if ((param_1 == 0) || (param_1 == 1)) {
    if (param_1 == 1) {
      piVar8 = (int *)&gp0xffffa820;
      iVar9 = 0x6aba40;
      piVar7 = (int *)&gp0xffffa81c;
    }
    else {
      if (param_1 != 0) {
        return 0;
      }
      piVar8 = (int *)&gp0xffffa82c;
      iVar9 = 0x6af670;
      piVar7 = (int *)&gp0xffffa828;
    }
    if ((((-1 < *piVar8) && (*piVar8 < 2)) && (-1 < *piVar7)) && ((*piVar7 < 8 && (iVar9 != 0)))) {
      iVar10 = 0x1e;
      do {
        for (; (lVar2 = FUN_002236b0(0), lVar2 != 0 && (0 < iVar10)); iVar10 = iVar10 + -1) {
        }
        iStack_4 = *piVar8;
        pcVar3 = (char *)(iVar9 + 0x13);
        iVar10 = 0;
        iVar4 = *piVar7 * 0x3c30 + iStack_4 * 0x1e218 + 0x660640;
        pcVar6 = (char *)(iVar4 + 0xa3);
        do {
          if (*pcVar3 != *pcVar6) {
            bVar1 = false;
            goto LAB_00243bb8;
          }
          iVar10 = iVar10 + 1;
          pcVar3 = pcVar3 + 1;
          pcVar6 = pcVar6 + 1;
        } while (iVar10 < 6);
        bVar1 = true;
LAB_00243bb8:
        if (bVar1) {
          pcVar3 = (char *)(iVar4 + 0x98);
          pcVar6 = (char *)(iVar9 + 8);
          iVar10 = 0;
          do {
            if (*pcVar6 != *pcVar3) {
              bVar1 = false;
              goto LAB_00243c08;
            }
            iVar10 = iVar10 + 1;
            pcVar6 = pcVar6 + 1;
            pcVar3 = pcVar3 + 1;
          } while (iVar10 < 0xb);
          bVar1 = true;
LAB_00243c08:
          if (bVar1) {
            bVar1 = *(int *)(iVar9 + 0x3c2c) == *(int *)(iVar4 + 0x3cbc);
          }
          else {
            bVar1 = false;
          }
        }
        else {
          bVar1 = false;
        }
        if (bVar1) {
          return 1;
        }
        iVar10 = 0x49;
        iStack_8 = *piVar7;
        while( true ) {
          iVar10 = iVar10 + -1;
          lVar2 = FUN_00243f90(&iStack_4,&iStack_8);
          if (lVar2 == 0) break;
          iVar4 = (int)lVar2;
          pcVar3 = (char *)(iVar4 + 0x13);
          pcVar6 = (char *)(iVar9 + 0x13);
          iVar5 = 0;
          do {
            if (*pcVar6 != *pcVar3) {
              bVar1 = false;
              goto LAB_00243ca8;
            }
            iVar5 = iVar5 + 1;
            pcVar6 = pcVar6 + 1;
            pcVar3 = pcVar3 + 1;
          } while (iVar5 < 6);
          bVar1 = true;
LAB_00243ca8:
          if (bVar1) {
            pcVar3 = (char *)(iVar4 + 8);
            pcVar6 = (char *)(iVar9 + 8);
            iVar5 = 0;
            do {
              if (*pcVar6 != *pcVar3) {
                bVar1 = false;
                goto LAB_00243cf8;
              }
              iVar5 = iVar5 + 1;
              pcVar6 = pcVar6 + 1;
              pcVar3 = pcVar3 + 1;
            } while (iVar5 < 0xb);
            bVar1 = true;
LAB_00243cf8:
            if (bVar1) {
              bVar1 = *(int *)(iVar9 + 0x3c2c) == *(int *)(iVar4 + 0x3c2c);
            }
            else {
              bVar1 = false;
            }
          }
          else {
            bVar1 = false;
          }
          if ((((iStack_4 == *piVar8) && (iStack_8 == *piVar7)) || (bVar1)) || (iVar10 < 1))
          goto LAB_00243d48;
        }
        bVar1 = false;
LAB_00243d48:
        if (bVar1) {
          *piVar8 = iStack_4;
          *piVar7 = iStack_8;
          return 1;
        }
        if (*(int *)(*piVar8 * 0x1e218 + 0x660640) == 1) {
          FUN_0027ca80(iVar9 + 8,param_1);
          FUN_0027b490();
          if (iGpffffa958 == 2) {
            return 0;
          }
        }
        else {
          FUN_0027cde0(iVar9 + 8,param_1);
          while (iGpffffa950 == 0) {
            uGpffff9e68 = 0x3f800000;
            (**(code **)(*piGpffff9e64 + 0x14))();
          }
          FUN_0027b490();
          if (iGpffffa950 == 2) {
            return 0;
          }
        }
        iVar10 = 0x1e;
      } while( true );
    }
  }
  return 0;
}

