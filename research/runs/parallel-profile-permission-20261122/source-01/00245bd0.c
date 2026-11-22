
undefined4 FUN_00245bd0(int param_1,long param_2)

{
  undefined4 uVar1;
  
  if (param_2 != 0) {
    *(undefined4 *)param_2 = 0x800;
  }
  if (param_1 == 1) {
    uVar1 = 0x6ac370;
  }
  else if (param_1 == 0) {
    uVar1 = 0x6affa0;
  }
  else {
    uVar1 = 0;
  }
  return uVar1;
}

