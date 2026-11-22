
int FUN_004eef00(undefined8 param_1)

{
  byte *pbVar1;
  int iVar2;
  uint uVar3;
  byte *pbVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  undefined1 auStack_4 [4];
  
  pbVar1 = (byte *)FUN_00245bd0(param_1,auStack_4);
  pbVar4 = pbVar1 + 4;
  if ((*pbVar1 & 1) == 0) {
    iVar2 = 0x19;
  }
  else {
    iVar2 = 3;
    uVar5 = 0;
    do {
      if (*(int *)(pbVar4 + 4) == 0) {
        switch(uVar5) {
        case 0:
        case 1:
        case 3:
        case 5:
        case 6:
        case 8:
          iVar2 = iVar2 + 1;
        }
      }
      else {
        iVar2 = iVar2 + 1;
      }
      uVar5 = uVar5 + 1;
      pbVar4 = pbVar4 + 8;
    } while (uVar5 < 0x10);
    uVar5 = 0;
    do {
      pbVar4 = pbVar1 + 0x200;
      uVar6 = 0;
      uVar7 = 0;
      do {
        if (*(short *)(pbVar4 + 2) != 0) {
          uVar3 = 1 << (*pbVar4 & 0x1f);
          if ((uVar6 & uVar3) == 0) {
            iVar2 = iVar2 + 1;
          }
          uVar6 = uVar6 | uVar3;
        }
        uVar7 = uVar7 + 1;
        pbVar4 = pbVar4 + 4;
      } while (uVar7 < 0x14);
      uVar5 = uVar5 + 1;
      pbVar1 = pbVar1 + 0x6c;
    } while (uVar5 < 2);
    iVar2 = iVar2 + 0x14;
  }
  return iVar2;
}

