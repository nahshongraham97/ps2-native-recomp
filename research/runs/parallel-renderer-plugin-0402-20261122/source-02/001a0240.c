
undefined8 FUN_001a0240(undefined8 param_1,float *param_2)

{
  bool bVar1;
  bool bVar2;
  bool bVar3;
  uint uVar4;
  float *pfVar5;
  float fVar6;
  float fVar7;
  float fVar8;
  float fVar9;
  float fVar10;
  float fVar11;
  float fVar12;
  float fVar13;
  float fVar14;
  
  pfVar5 = (float *)param_1;
  if (param_2 == (float *)0x0) {
    param_2 = (float *)(iRam0064e478 + 0x69f29c);
    fVar6 = pfVar5[1];
  }
  else {
    fVar6 = pfVar5[1];
  }
  fVar14 = pfVar5[5];
  fVar7 = (pfVar5[2] * pfVar5[2] + *pfVar5 * *pfVar5 + fVar6 * fVar6) - 1.0;
  fVar12 = pfVar5[4];
  fVar9 = pfVar5[6];
  fVar13 = pfVar5[9];
  fVar8 = (fVar9 * fVar9 + fVar12 * fVar12 + fVar14 * fVar14) - 1.0;
  fVar11 = pfVar5[8];
  fVar10 = pfVar5[10];
  fVar6 = (fVar10 * fVar10 + fVar11 * fVar11 + fVar13 * fVar13) - 1.0;
  bVar1 = fVar6 * fVar6 + fVar7 * fVar7 + fVar8 * fVar8 <= *param_2;
  fVar8 = fVar9 * fVar10 + fVar12 * fVar11 + fVar14 * fVar13;
  fVar6 = fVar10 * pfVar5[2] + fVar11 * *pfVar5 + fVar13 * pfVar5[1];
  fVar7 = pfVar5[2] * fVar9 + *pfVar5 * fVar12 + pfVar5[1] * fVar14;
  bVar2 = fVar7 * fVar7 + fVar8 * fVar8 + fVar6 * fVar6 <= param_2[1];
  bVar3 = false;
  if (bVar1 && bVar2) {
    bVar3 = pfVar5[0xe] * pfVar5[0xe] + pfVar5[0xc] * pfVar5[0xc] + pfVar5[0xd] * pfVar5[0xd] +
            (fVar10 - 1.0) * (fVar10 - 1.0) + fVar11 * fVar11 + fVar13 * fVar13 +
            pfVar5[2] * pfVar5[2] + (*pfVar5 - 1.0) * (*pfVar5 - 1.0) + pfVar5[1] * pfVar5[1] +
            fVar9 * fVar9 + fVar12 * fVar12 + (fVar14 - 1.0) * (fVar14 - 1.0) <= param_2[2];
    fVar6 = pfVar5[3];
  }
  else {
    fVar6 = pfVar5[3];
  }
  if (bVar1) {
    uVar4 = (uint)fVar6 | 1;
  }
  else {
    uVar4 = (uint)fVar6 & 0xfffffffe;
  }
  if (bVar2) {
    uVar4 = uVar4 | 2;
  }
  else {
    uVar4 = uVar4 & 0xfffffffd;
  }
  if (bVar3) {
    fVar6 = (float)(uVar4 | 0x20000);
  }
  else {
    fVar6 = (float)(uVar4 & 0xfffdffff);
  }
  pfVar5[3] = fVar6;
  return param_1;
}

