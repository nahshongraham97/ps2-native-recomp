
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_002be230(undefined4 param_1)

{
  if (*(int *)((int)&DAT_00835340 + DAT_005c73ac) != 0) {
    FUN_002c3fc0(*(int *)((int)&DAT_00835340 + DAT_005c73ac));
    *(undefined4 *)((int)&DAT_00835340 + DAT_005c73ac) = 0;
  }
  _DAT_005c73b0 = _DAT_005c73b0 + -1;
  return param_1;
}

