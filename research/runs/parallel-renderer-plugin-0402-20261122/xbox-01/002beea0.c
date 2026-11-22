
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_002beea0(undefined4 param_1,int param_2)

{
  undefined4 uVar1;
  int iVar2;
  int iVar3;
  
  DAT_005c73ac = param_2;
  iVar3 = FUN_002c4620(0x40,DAT_005a6760,0x10,DAT_005a6764,&DAT_005c73b4,0x4000d);
  iVar2 = DAT_005c73ac;
  *(int *)((int)&DAT_00835340 + DAT_005c73ac) = iVar3;
  uVar1 = DAT_004d8774;
  if (iVar3 == 0) {
    return 0;
  }
  *(undefined4 *)((int)&DAT_00835344 + iVar2) = 0x20000;
  *(undefined1 **)((int)&DAT_00835348 + iVar2) = &LAB_002bd250;
  *(undefined4 *)(&DAT_0083534c + iVar2) = uVar1;
  *(undefined4 *)((int)&DAT_00835350 + iVar2) = uVar1;
  *(undefined4 *)((int)&DAT_00835354 + iVar2) = uVar1;
  _DAT_005c73b0 = _DAT_005c73b0 + 1;
  return param_1;
}

