
undefined4 FUN_00516220(undefined8 param_1)

{
  int iVar1;
  undefined4 uVar2;
  long lVar3;
  int iVar4;
  uint uVar5;
  int iVar6;
  
  uVar5 = 0;
  iVar4 = 0x7bd230;
  do {
    if (*(uint *)(iVar4 + 0xac) == (*(uint *)((int)param_1 + 0xc) | 0x10000000)) {
      iVar4 = uVar5 * 0xa28 + 0x7bd2dc;
      goto LAB_0051629c;
    }
    uVar5 = uVar5 + 1;
    iVar4 = iVar4 + 0xa28;
  } while (uVar5 < 4);
  iVar4 = 0;
LAB_0051629c:
  if (iVar4 == 0) {
    uVar2 = 0;
  }
  else {
    uVar5 = 0;
    iVar6 = iVar4;
    if (*(int *)(iVar4 + 0x170) != 0) {
      do {
        lVar3 = FUN_0049fbf0(param_1,*(undefined1 *)(iVar6 + 0x18e));
        if (lVar3 != 0) {
          iVar1 = FUN_004a0480(lVar3,*(undefined4 *)((int)param_1 + 0x10),
                               *(undefined1 *)(iVar6 + 0x18e));
          *(undefined4 *)(iVar1 + 0xcc) = *(undefined4 *)(iVar6 + 0x17c);
          *(undefined4 *)(iVar1 + 0xd4) = *(undefined4 *)(iVar6 + 0x180);
          *(undefined4 *)(iVar1 + 0xd0) = 0x44610000;
          *(byte *)(iVar1 + 0x7b) = *(byte *)(iVar1 + 0x7b) & 0xfd | 2;
          *(undefined4 *)(iVar1 + 0xe0) = 0;
          *(undefined4 *)(iVar1 + 0xdc) = 0;
          *(undefined4 *)(iVar1 + 0xd8) = 0;
          *(undefined4 *)(iVar1 + 0xdc) = *(undefined4 *)(iVar6 + 0x184);
          *(uint *)(iVar1 + 0x170) = (uint)*(byte *)(iVar6 + 0x174);
          *(undefined4 *)(iVar1 + 0x188) = *(undefined4 *)(iVar6 + 0x178);
          *(uint *)(iVar1 + 0x174) = (uint)*(byte *)(iVar6 + 0x175);
          *(undefined4 *)(iVar1 + 0x1e0) = *(undefined4 *)(iVar6 + 0x188);
          *(uint *)(iVar1 + 0x278) = (uint)*(byte *)(iVar6 + 0x18c);
          *(uint *)(iVar1 + 0xc0) = (uint)*(byte *)(iVar6 + 0x18d);
        }
        uVar5 = uVar5 + 1;
        iVar6 = iVar6 + 0x1c;
      } while (uVar5 < *(uint *)(iVar4 + 0x170));
    }
    uVar2 = 1;
  }
  return uVar2;
}

