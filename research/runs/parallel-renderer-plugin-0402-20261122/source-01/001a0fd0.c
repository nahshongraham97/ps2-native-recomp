
long thunk_FUN_001a0e00(int param_1,uint param_2,long param_3,int param_4,long param_5,ulong param_6
                       )

{
  uint *puVar1;
  long lVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;
  uint *puVar6;
  
  if (iGpffff8368 == 0) {
    param_4 = 0;
  }
  if (param_3 == 0) {
    param_3 = 0x10;
  }
  if (param_5 == 0) {
    if (iGpffffa394 == 0) {
      param_5 = (*pcRam0069f3c4)(0x24,param_6 & 0xff0000);
    }
    else {
      param_5 = (*pcRam0069f3d4)(iGpffffa394,param_6 & 0xff0000);
    }
    if (param_5 == 0) {
      return 0;
    }
    *(undefined4 *)((int)param_5 + 0x18) = 2;
  }
  else {
    *(undefined4 *)((int)param_5 + 0x18) = 3;
  }
  uVar4 = (uint)param_3;
  uVar3 = ~(uVar4 - 1) & param_1 + (uVar4 - 1);
  puVar6 = (uint *)param_5;
  *puVar6 = uVar3;
  puVar6[1] = param_2;
  uVar5 = param_2 + 7 >> 3;
  puVar6[3] = uVar4;
  puVar6[2] = uVar5;
  puVar6[4] = (uint)(puVar6 + 4);
  puVar6[5] = (uint)(puVar6 + 4);
  if (param_4 != 0) {
    do {
      lVar2 = (*pcRam0069f3c4)(uVar4 + uVar5 + 7 + param_2 * uVar3,param_6);
      if (lVar2 == 0) {
        FUN_001a0fe0(param_5);
        return 0;
      }
      puVar1 = (uint *)lVar2;
      puVar1[1] = 0;
      *puVar1 = 0;
      *puVar1 = puVar6[4];
      puVar1[1] = (uint)(puVar6 + 4);
      *(uint **)(puVar6[4] + 4) = puVar1;
      puVar6[4] = (uint)puVar1;
      FUN_001dba60(puVar1 + 2,0,uVar5);
      param_4 = param_4 + -1;
    } while (param_4 != 0);
  }
  puVar6[7] = (uint)puGpffffa398;
  puVar6[8] = (uint)&puGpffffa398;
  *(uint **)((int)puGpffffa398 + 4) = puVar6 + 7;
  puGpffffa398 = puVar6 + 7;
  return param_5;
}

