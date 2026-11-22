
void FUN_0026f070(undefined4 *param_1)

{
  undefined4 *puVar1;
  undefined8 uVar2;
  
  if (puRam0064e998 != param_1) {
    if (puRam0064e998 == (undefined4 *)0x0) {
      puVar1 = (undefined4 *)*param_1;
    }
    else {
      FUN_003de4c0();
      if (iRam0064e99c != 0) {
        FUN_00219280();
        iRam0064e99c = 0;
      }
      puRam0064e998 = (undefined4 *)0x0;
      puVar1 = (undefined4 *)*param_1;
    }
    uVar2 = FUN_002209e0(*puVar1,1);
    iRam0064e99c = FUN_00219290(uRam0064e818,uVar2);
    puRam0064e998 = param_1;
  }
  return;
}

