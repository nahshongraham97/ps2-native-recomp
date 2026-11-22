
void FUN_003d0520(undefined8 param_1,long param_2)

{
  bool bVar1;
  long lVar2;
  int iVar3;
  int iVar4;
  undefined1 *puVar5;
  int iVar6;
  
  if (((iRam0064f118 != 0x18) && (bVar1 = false, *(uint *)(iRam0064f110 + 0x1eb40) < 0x1e)) &&
     (lVar2 = FUN_001deb10(param_2,0x63ec40), lVar2 != 0)) {
    iVar3 = *(int *)(iRam0064f110 + 0x1eb40);
    iVar6 = 0;
    if (0 < iVar3) {
      iVar4 = 0;
      do {
        lVar2 = FUN_001deb10(iRam0064f110 + iVar4 + 0x1dcb8,param_2);
        if (lVar2 == 0) {
          bVar1 = true;
        }
        iVar6 = iVar6 + 1;
        iVar3 = *(int *)(iRam0064f110 + 0x1eb40);
        iVar4 = iVar4 + 0x7c;
      } while (iVar6 < iVar3);
    }
    if (!bVar1) {
      *(undefined4 *)(iRam0064f110 + iVar3 * 0x7c + 0x1dcc4) = 2;
      iVar3 = *(int *)(iRam0064f110 + 0x1eb40);
      *(int *)(iRam0064f110 + 0x1eb40) = iVar3 + 1;
      iVar3 = iRam0064f110 + iVar3 * 0x7c;
      puVar5 = (undefined1 *)(iVar3 + 0x1dcb8);
      *puVar5 = 0;
      if (param_2 != 0) {
        FUN_001df378(puVar5,param_2,8);
        *(undefined1 *)(iVar3 + 0x1dcc0) = 0;
      }
    }
  }
  return;
}

