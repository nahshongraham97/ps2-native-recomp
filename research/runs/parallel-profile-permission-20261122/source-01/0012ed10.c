
int FUN_0012ed10(int param_1)

{
  int iVar1;
  int *piVar2;
  int *piVar3;
  
  if (piRam0064df60 != (int *)0x0) {
    iVar1 = *piRam0064df60;
    piVar3 = piRam0064df60;
    while( true ) {
      if (piVar3[4] == *(int *)(iVar1 + 4)) {
        if (*(int *)(iVar1 + 8) == param_1) {
          return iVar1;
        }
        piVar2 = (int *)piVar3[1];
      }
      else {
        piVar2 = (int *)piVar3[1];
        piVar3[4] = 1;
        FUN_0012fa00();
      }
      if (piVar2 == (int *)0x0) break;
      iVar1 = *piVar2;
      piVar3 = piVar2;
    }
  }
  return 0;
}

