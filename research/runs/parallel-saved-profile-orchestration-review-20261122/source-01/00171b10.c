
long FUN_00171b10(void)

{
  long lVar1;
  int iVar2;
  int iVar3;
  undefined4 *puVar4;
  int iVar5;
  int iVar6;
  
  lVar1 = FUN_00225000();
  if (lVar1 != 0) {
    iVar6 = 0;
    puVar4 = (undefined4 *)0x660640;
    do {
      if ((-1 < iVar6) && (iVar6 < 2)) {
        iVar5 = 0;
        *puVar4 = 0xffffffff;
        iVar3 = iVar6 * 0x1e218 + 0x660640;
        iVar2 = iVar3;
        do {
          *(undefined4 *)(iVar3 + 0x18) = 0;
          FUN_00245cb0(iVar2 + 0x90);
          iVar5 = iVar5 + 1;
          iVar3 = iVar3 + 4;
          iVar2 = iVar2 + 0x3c30;
        } while (iVar5 < 8);
        *(undefined1 *)(puVar4 + 3) = 0;
        FUN_002432a0();
        FUN_002955d0(puVar4 + 0xe);
      }
      iVar6 = iVar6 + 1;
      puVar4 = puVar4 + 0x7886;
    } while (iVar6 < 2);
    uRam00660640 = 1;
    uRam0067e858 = 1;
  }
  return lVar1;
}

