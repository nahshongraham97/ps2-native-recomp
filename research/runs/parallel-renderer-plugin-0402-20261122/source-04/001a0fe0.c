
void FUN_001a0fe0(int param_1)

{
  int iVar1;
  int *piVar2;
  
  piVar2 = *(int **)(param_1 + 0x10);
  if (piVar2 != (int *)(param_1 + 0x10)) {
    iVar1 = *piVar2;
    while( true ) {
      *(int *)piVar2[1] = iVar1;
      *(int *)(*piVar2 + 4) = piVar2[1];
      (*pcRam0069f3c8)();
      piVar2 = *(int **)(param_1 + 0x10);
      if (piVar2 == (int *)(param_1 + 0x10)) break;
      iVar1 = *piVar2;
    }
  }
  if ((*(uint *)(param_1 + 0x18) & 1) == 0) {
    if ((iGpffffa394 == param_1) || (iGpffffa394 == 0)) {
      (*pcRam0069f3c8)(param_1);
    }
    else {
      (*pcRam0069f3d8)(iGpffffa394,param_1);
    }
  }
  return;
}

