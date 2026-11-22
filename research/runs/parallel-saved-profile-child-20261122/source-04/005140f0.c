
void FUN_005140f0(void)

{
  undefined4 uVar1;
  undefined4 uVar2;
  int iVar3;
  int iVar4;
  uint uVar5;
  
  uVar5 = 0;
  iVar4 = 0x7bd230;
  bRam007bfc40 = bRam007bfc40 & 0xfe;
  iVar3 = 0x7bfc40;
  do {
    uVar5 = uVar5 + 5;
    uVar1 = *(undefined4 *)(iVar4 + 0x2964);
    uVar2 = *(undefined4 *)(iVar4 + 0x2968);
    *(undefined4 *)(iVar3 + 0x2960) = *(undefined4 *)(iVar4 + 0x2960);
    *(undefined4 *)(iVar3 + 0x2964) = uVar1;
    *(undefined4 *)(iVar3 + 0x2968) = uVar2;
    uVar1 = *(undefined4 *)(iVar4 + 0x2970);
    *(undefined4 *)(iVar3 + 0x296c) = *(undefined4 *)(iVar4 + 0x296c);
    *(undefined4 *)(iVar3 + 0x2970) = uVar1;
    iVar4 = iVar4 + 0x14;
    iVar3 = iVar3 + 0x14;
  } while (uVar5 < 10);
  uRam007c25d8 = uRam007bfc24;
  uRam007c25d4 = uRam007bfc24;
  uRam007c25dc = uRam007bfc2c;
  uRam007c25e0 = uRam007bfc2c;
  FUN_00515240(uRam007bfc54,1);
  uRam007bfc54 = uRam007bfc54 + 1;
  if (7 < uRam007bfc54) {
    uRam007bfc54 = 0;
  }
  FUN_00514240(uRam007bfc54);
  bRam007bfc40 = bRam007bfc40 & 0xf7;
  FUN_00431020();
  return;
}

