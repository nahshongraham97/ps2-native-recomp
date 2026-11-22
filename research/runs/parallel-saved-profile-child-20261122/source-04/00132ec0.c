
void FUN_00132ec0(int param_1,undefined8 param_2,undefined4 *param_3)

{
  undefined4 *puVar1;
  undefined1 auStack_10 [16];
  
  FUN_00156670(auStack_10,param_2,param_1 + 0xc0);
  puVar1 = (undefined4 *)param_2;
  *(undefined4 *)(param_1 + 0xc0) = *puVar1;
  *(undefined4 *)(param_1 + 0xc4) = puVar1[1];
  *(undefined4 *)(param_1 + 200) = puVar1[2];
  *(undefined4 *)(param_1 + 0xf0) = *param_3;
  *(undefined4 *)(param_1 + 0xf4) = param_3[1];
  *(undefined4 *)(param_1 + 0xf8) = param_3[2];
  FUN_0013c3c0(param_1);
  FUN_0016e690(param_1,auStack_10);
  if (param_1 == iRam00660160) {
    FUN_0019e190(*(undefined4 *)(iRam0066015c + 0x2f4),param_2);
  }
  else {
    FUN_0019e190(*(undefined4 *)(iRam006601a4 + 0x2f4),param_2);
  }
  return;
}

