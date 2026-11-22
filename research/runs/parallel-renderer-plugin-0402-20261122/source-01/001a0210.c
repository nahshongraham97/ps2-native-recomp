
undefined4 FUN_001a0210(undefined4 *param_1)

{
  int iVar1;
  undefined4 uVar2;
  undefined4 uVar3;
  
  iVar1 = iRam0064e478;
  uVar2 = param_1[1];
  uVar3 = param_1[2];
  *(undefined4 *)(iRam0064e478 + 0x69f29c) = *param_1;
  *(undefined4 *)(iVar1 + 0x69f2a0) = uVar2;
  *(undefined4 *)(iVar1 + 0x69f2a4) = uVar3;
  return 1;
}

