
undefined8 FUN_001a07a0(undefined8 param_1,float *param_2)

{
  float fVar1;
  float *extraout_a0_lo;
  float *pfVar2;
  undefined4 extraout_a0_hi;
  undefined4 uVar3;
  int iVar4;
  float *pfVar5;
  
  uVar3 = (undefined4)((ulong)param_1 >> 0x20);
  pfVar2 = (float *)param_1;
  if (((uint)param_2[3] & *(uint *)(iRam0064e478 + 0x69f294) & 0x20000) == 0) {
    if (((uint)param_2[3] & 3) == 3) {
      *pfVar2 = *param_2;
      pfVar2[1] = param_2[4];
      pfVar2[2] = param_2[8];
      pfVar2[4] = param_2[1];
      pfVar2[5] = param_2[5];
      pfVar2[6] = param_2[9];
      pfVar2[8] = param_2[2];
      pfVar2[9] = param_2[6];
      pfVar2[10] = param_2[10];
      pfVar2[0xc] = -(param_2[0xe] * param_2[2] +
                     param_2[0xc] * *param_2 + param_2[0xd] * param_2[1]);
      pfVar2[0xd] = -(param_2[0xe] * param_2[6] +
                     param_2[0xc] * param_2[4] + param_2[0xd] * param_2[5]);
      pfVar2[0xe] = -(param_2[0xe] * param_2[10] +
                     param_2[0xc] * param_2[8] + param_2[0xd] * param_2[9]);
      pfVar2[3] = 4.2039e-45;
    }
    else {
      FUN_0019fd60();
      pfVar2 = extraout_a0_lo;
      uVar3 = extraout_a0_hi;
    }
  }
  else {
    iVar4 = 8;
    pfVar5 = pfVar2;
    do {
      iVar4 = iVar4 + -1;
      fVar1 = param_2[1];
      *pfVar5 = *param_2;
      param_2 = param_2 + 2;
      pfVar5[1] = fVar1;
      pfVar5 = pfVar5 + 2;
    } while (0 < iVar4);
  }
  return CONCAT44(uVar3,pfVar2);
}

