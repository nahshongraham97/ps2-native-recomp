
bool FUN_001892d0(int param_1,long param_2,long param_3)

{
  long lVar1;
  ulong uVar2;
  
  if (0x3f < (ulong)(long)param_1) {
    return true;
  }
  if (param_1 == 0x3e) {
    if ((param_3 != 5) && (lVar1 = FUN_004eee60(param_3), lVar1 != 0)) {
      return true;
    }
    if (param_2 != 0) {
      return true;
    }
  }
  if (param_2 == 0) {
    uVar2 = uRam00660508 | uGpffff8348;
  }
  else {
    uVar2 = uRam00660510 | uGpffffa228;
  }
  return (uVar2 & 1L << (long)param_1) == 0;
}

