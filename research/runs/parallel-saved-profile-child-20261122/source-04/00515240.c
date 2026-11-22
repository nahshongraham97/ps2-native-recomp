
void FUN_00515240(ulong param_1,int param_2)

{
  uint uVar1;
  uint uVar2;
  uint *puVar3;
  uint uVar4;
  
  uVar1 = (uint)param_1;
  uVar2 = uVar1 >> 5;
  uVar4 = 1 << (uVar1 & 0x1f);
  if (uVar2 < 0xb) {
    if (param_2 == 0) {
      puVar3 = (uint *)(uVar2 * 4 + 0x7bfb90);
      *puVar3 = *puVar3 & ~uVar4;
    }
    else {
      puVar3 = (uint *)(uVar2 * 4 + 0x7bfb90);
      *puVar3 = *puVar3 | uVar4;
      if (uVar1 - 0xd < 3) {
        puVar3 = (uint *)(uVar2 * 4 + 0x7c25a0);
        *puVar3 = *puVar3 | uVar4;
      }
    }
    if ((param_2 == 1) && ((param_1 < 8 || (uVar1 - 0xd < 2)))) {
      uRam006af76c = uRam006af76c | 1 << (uVar1 & 0x1f);
    }
  }
  return;
}

