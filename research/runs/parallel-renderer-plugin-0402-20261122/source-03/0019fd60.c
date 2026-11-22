
undefined8 FUN_0019fd60(undefined8 param_1,float *param_2)

{
  float *pfVar1;
  float fVar2;
  
  pfVar1 = (float *)param_1;
  *pfVar1 = param_2[5] * param_2[10] - param_2[6] * param_2[9];
  pfVar1[1] = -(param_2[1] * param_2[10] - param_2[2] * param_2[9]);
  fVar2 = param_2[1] * param_2[6] - param_2[2] * param_2[5];
  pfVar1[2] = fVar2;
  fVar2 = fVar2 * param_2[8] + *pfVar1 * *param_2 + pfVar1[1] * param_2[4];
  if (fVar2 == 0.0) {
    fVar2 = 1.0;
  }
  else {
    fVar2 = 1.0 / fVar2;
  }
  *pfVar1 = *pfVar1 * fVar2;
  pfVar1[1] = pfVar1[1] * fVar2;
  pfVar1[2] = pfVar1[2] * fVar2;
  pfVar1[4] = fVar2 * -(param_2[4] * param_2[10] - param_2[6] * param_2[8]);
  pfVar1[5] = fVar2 * (*param_2 * param_2[10] - param_2[2] * param_2[8]);
  pfVar1[6] = fVar2 * -(*param_2 * param_2[6] - param_2[2] * param_2[4]);
  pfVar1[8] = fVar2 * (param_2[4] * param_2[9] - param_2[5] * param_2[8]);
  pfVar1[9] = fVar2 * -(*param_2 * param_2[9] - param_2[1] * param_2[8]);
  pfVar1[10] = fVar2 * (*param_2 * param_2[5] - param_2[1] * param_2[4]);
  pfVar1[0xc] = -(param_2[0xe] * pfVar1[8] + param_2[0xc] * *pfVar1 + param_2[0xd] * pfVar1[4]);
  pfVar1[0xd] = -(param_2[0xe] * pfVar1[9] + param_2[0xc] * pfVar1[1] + param_2[0xd] * pfVar1[5]);
  pfVar1[0xe] = -(param_2[0xe] * pfVar1[10] + param_2[0xc] * pfVar1[2] + param_2[0xd] * pfVar1[6]);
  pfVar1[3] = 0.0;
  return param_1;
}

