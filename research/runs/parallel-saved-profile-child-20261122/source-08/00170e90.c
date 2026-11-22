
void FUN_00170e90(undefined1 *param_1,undefined1 *param_2)

{
  int iVar1;
  
  iVar1 = 0;
  do {
    iVar1 = iVar1 + 5;
    *param_2 = *param_1;
    param_2[1] = param_1[1];
    param_2[2] = param_1[2];
    param_2[3] = param_1[3];
    param_2[4] = param_1[4];
    param_1 = param_1 + 5;
    param_2 = param_2 + 5;
  } while (iVar1 < 6);
  *param_2 = *param_1;
  return;
}

