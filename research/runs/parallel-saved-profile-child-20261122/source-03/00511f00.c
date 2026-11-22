
void FUN_00511f00(int param_1)

{
  int *piVar1;
  int iVar2;
  uint uVar3;
  long lVar4;
  uint *puVar5;
  int iVar6;
  int *piVar7;
  uint uVar8;
  uint uVar9;
  uint *puVar10;
  
  if (((long)((ulong)*(byte *)(param_1 + 8) << 0x3d) < 0) && (*(uint *)(param_1 + 0xc) < 0x30)) {
    uVar8 = 0;
    iVar6 = 0x7bd230;
    uVar9 = *(uint *)(param_1 + 0xc) | 0x10000000;
    do {
      if (*(uint *)(iVar6 + 0xac) == uVar9) {
        puVar10 = (uint *)(uVar8 * 0xa28 + 0x7bd2dc);
        goto LAB_00511f94;
      }
      uVar8 = uVar8 + 1;
      iVar6 = iVar6 + 0xa28;
    } while (uVar8 < 4);
    puVar10 = (uint *)0x0;
LAB_00511f94:
    if (puVar10 == (uint *)0x0) {
      uVar8 = 0;
      iVar6 = 0x7bd230;
      do {
        if ((*(uint *)(iVar6 + 0xac) & 0x10000000) != 0x10000000) {
          puVar10 = (uint *)(uVar8 * 0xa28 + 0x7bd2dc);
          break;
        }
        uVar8 = uVar8 + 1;
        iVar6 = iVar6 + 0xa28;
      } while (uVar8 < 4);
    }
    if (puVar10 == (uint *)0x0) {
      puVar10 = (uint *)0x7bd2dc;
    }
    FUN_001dba60(puVar10,0,0xa28);
    *puVar10 = uVar9;
    if ((param_1 != -0xe7c) && (piVar7 = *(int **)(param_1 + 0xe7c), piVar7 != (int *)0x0)) {
      iVar6 = *piVar7;
      do {
        if (piVar7[4] == *(int *)(iVar6 + 4)) {
          if ((*(uint *)(*(int *)(iVar6 + 0x3c) + 0x28) & 0x40) == 0) {
            uVar8 = 0;
            do {
              iVar2 = *(int *)(iVar6 + 0x2c);
              if (iVar2 != 0) {
                puVar5 = *(uint **)(iVar2 + 0x1c);
                uVar9 = 0;
                if (*(int *)(iVar2 + 0x10) != 0) {
                  do {
                    if (((long)((ulong)(byte)*puVar5 << 0x3d) < 0) &&
                       (-1 < (long)((ulong)(byte)*puVar5 << 0x3e))) {
                      uVar3 = puVar10[1];
                      if (0x1d < uVar3) break;
                      puVar10[1] = uVar3 + 1;
                      if (0x1d < uVar3 + 1) break;
                      puVar10[uVar3 * 3 + 2] = *(uint *)puVar5[0x10];
                      *(char *)((int)puVar10 + uVar3 * 0xc + 0xd) = (char)uVar8;
                      *(char *)(puVar10 + uVar3 * 3 + 3) = (char)puVar5[5];
                      puVar10[uVar3 * 3 + 4] = *puVar5;
                    }
                    uVar9 = uVar9 + 1;
                    puVar5 = puVar5 + 0x12;
                  } while (uVar9 < *(uint *)(*(int *)(iVar6 + 0x2c) + 0x10));
                }
              }
              uVar8 = uVar8 + 1;
              iVar6 = iVar6 + 4;
            } while (uVar8 < 4);
          }
          piVar1 = (int *)piVar7[1];
        }
        else {
          piVar1 = (int *)piVar7[1];
          piVar7[4] = 1;
          FUN_0012fa00();
        }
        if (piVar1 == (int *)0x0) break;
        iVar6 = *piVar1;
        piVar7 = piVar1;
      } while( true );
    }
    if (((int *)(iGpffffb160 + 0x7c4) != (int *)0x0) &&
       (piVar7 = *(int **)(iGpffffb160 + 0x7c4), piVar7 != (int *)0x0)) {
      iVar6 = *piVar7;
      do {
        if (piVar7[4] == *(int *)(iVar6 + 4)) {
          if ((*(int *)(iVar6 + 0xc4) == *(int *)(param_1 + 0x10)) &&
             (0.0 < *(float *)(iVar6 + 0x1e0))) {
            uVar8 = puVar10[0x5c];
            puVar10[0x5c] = uVar8 + 1;
            if (0x13 < uVar8 + 1) break;
            *(char *)((int)puVar10 + uVar8 * 0x1c + 0x18e) = (char)*(undefined4 *)(iVar6 + 0x27c);
            *(char *)((int)puVar10 + uVar8 * 0x1c + 0x18d) = (char)*(undefined4 *)(iVar6 + 0xc0);
            puVar10[uVar8 * 7 + 0x5e] = *(uint *)(iVar6 + 0x188);
            *(char *)(puVar10 + uVar8 * 7 + 0x5d) = (char)*(undefined4 *)(iVar6 + 0x170);
            puVar10[uVar8 * 7 + 0x62] = *(uint *)(iVar6 + 0x1e0);
            *(char *)(puVar10 + uVar8 * 7 + 99) = (char)*(undefined4 *)(iVar6 + 0x278);
            *(char *)((int)puVar10 + uVar8 * 0x1c + 0x175) = (char)*(undefined4 *)(iVar6 + 0x174);
            puVar10[uVar8 * 7 + 0x61] = *(uint *)(iVar6 + 0xdc);
            puVar10[uVar8 * 7 + 0x5f] = *(uint *)(iVar6 + 0xcc);
            puVar10[uVar8 * 7 + 0x60] = *(uint *)(iVar6 + 0xd4);
          }
          piVar1 = (int *)piVar7[1];
        }
        else {
          piVar1 = (int *)piVar7[1];
          piVar7[4] = 1;
          FUN_0012fa00();
        }
        if (piVar1 == (int *)0x0) break;
        iVar6 = *piVar1;
        piVar7 = piVar1;
      } while( true );
    }
    if ((param_1 != -0xe84) && (piVar7 = *(int **)(param_1 + 0xe84), piVar7 != (int *)0x0)) {
      iVar6 = *piVar7;
      do {
        if (piVar7[4] == *(int *)(iVar6 + 4)) {
          if ((-1 < (long)((ulong)*(byte *)(iVar6 + 0xa0) << 0x3d)) &&
             (lVar4 = FUN_00216250(*(undefined4 *)(param_1 + 0xb24),*(undefined4 *)(iVar6 + 0xc),0),
             lVar4 != 0)) {
            uVar8 = puVar10[0xe9];
            puVar10[0xe9] = uVar8 + 1;
            if (0x31 < uVar8 + 1) break;
            *(char *)((int)puVar10 + uVar8 * 4 + 0x3aa) =
                 (char)*(undefined4 *)(*(int *)(iVar6 + 0xc) + 0x24);
            if (lVar4 == 0) {
              *(byte *)((int)puVar10 + uVar8 * 4 + 0x3aa) =
                   *(byte *)((int)puVar10 + uVar8 * 4 + 0x3aa) | 8;
              *(undefined2 *)(puVar10 + uVar8 + 0xea) = 0;
            }
            else {
              *(short *)(puVar10 + uVar8 + 0xea) = (short)lVar4;
            }
          }
          piVar1 = (int *)piVar7[1];
        }
        else {
          piVar1 = (int *)piVar7[1];
          piVar7[4] = 1;
          FUN_0012fa00(piVar7);
        }
        if (piVar1 == (int *)0x0) break;
        iVar6 = *piVar1;
        piVar7 = piVar1;
      } while( true );
    }
    if ((param_1 != -0xe8c) && (piVar7 = *(int **)(param_1 + 0xe8c), piVar7 != (int *)0x0)) {
      iVar6 = *piVar7;
      do {
        if (piVar7[4] == *(int *)(iVar6 + 4)) {
          uVar8 = **(uint **)(iVar6 + 8);
          if (((uVar8 & 0x10000) == 0) && ((uVar8 & 0x800) != 0)) {
            uVar8 = *(uint *)(iVar6 + 0xc);
            uVar9 = (uVar8 & 0xfff) >> 5;
            if ((uVar8 & 0xff00f000) != 0x4000000) {
              puVar10[uVar9 + 0x286] = puVar10[uVar9 + 0x286] | 1 << (uVar8 & 0x1f);
            }
          }
          else if ((uVar8 & 0x18000) == 0) {
            lVar4 = FUN_00216250(*(undefined4 *)(param_1 + 0xb24),*(uint **)(iVar6 + 8),0);
            if (lVar4 != 0) {
              uVar8 = puVar10[0x11c];
              puVar10[0x11c] = uVar8 + 1;
              if (0x15 < uVar8 + 1) {
                return;
              }
              *(char *)(puVar10 + uVar8 * 0x10 + 0x11d) = (char)*(undefined4 *)(iVar6 + 0xc);
              *(short *)((int)puVar10 + uVar8 * 0x40 + 0x476) = (short)lVar4;
              *(byte *)(puVar10 + uVar8 * 0x10 + 0x11e) =
                   (byte)puVar10[uVar8 * 0x10 + 0x11e] & 0xfb |
                   (byte)((uint)(((ulong)*(byte *)(iVar6 + 0x34) << 0x3b) >> 0x3f) << 2);
              *(byte *)(puVar10 + uVar8 * 0x10 + 0x11e) =
                   (byte)puVar10[uVar8 * 0x10 + 0x11e] & 0xfe | *(byte *)(iVar6 + 0x34) & 1;
              *(byte *)(puVar10 + uVar8 * 0x10 + 0x11e) =
                   (byte)puVar10[uVar8 * 0x10 + 0x11e] & 0xfd |
                   (byte)((uint)(((ulong)*(byte *)(iVar6 + 0x34) << 0x3e) >> 0x3f) << 1);
              *(byte *)(puVar10 + uVar8 * 0x10 + 0x11e) =
                   (byte)puVar10[uVar8 * 0x10 + 0x11e] & 0xf7 |
                   (byte)((uint)(((ulong)*(byte *)(iVar6 + 0x34) << 0x39) >> 0x3f) << 3);
              puVar10[uVar8 * 0x10 + 0x11f] = *(uint *)(iVar6 + 0x10);
              puVar10[uVar8 * 0x10 + 0x120] = *(uint *)(iVar6 + 0x14);
              puVar10[uVar8 * 0x10 + 0x121] = *(uint *)(iVar6 + 0x18);
              puVar10[uVar8 * 0x10 + 0x122] = *(uint *)(iVar6 + 0x1c);
              puVar10[uVar8 * 0x10 + 0x123] = *(uint *)(iVar6 + 0x2c);
              puVar10[uVar8 * 0x10 + 0x124] = *(uint *)(iVar6 + 0x6c);
              puVar10[uVar8 * 0x10 + 0x125] = *(uint *)(iVar6 + 0x68);
              puVar10[uVar8 * 0x10 + 0x126] = *(uint *)(iVar6 + 0x70);
              puVar10[uVar8 * 0x10 + 0x127] = *(uint *)(iVar6 + 0x98);
              FUN_001dec58(puVar10 + uVar8 * 0x10 + 0x128,iVar6 + 0x84);
            }
          }
          else if (((uVar8 & 0x10000) == 0) || ((uVar8 & 0x8000) != 0)) {
            uVar8 = puVar10[0x283];
            if (4 < uVar8) {
              return;
            }
            if (0xff < *(uint *)(iVar6 + 0x70)) {
              return;
            }
            puVar10[0x283] = uVar8 + 1;
            *(char *)((int)puVar10 + uVar8 + 0xa10) = (char)*(undefined4 *)(iVar6 + 0x70);
          }
          else {
            uVar8 = puVar10[0x27d];
            if (0x13 < uVar8) {
              return;
            }
            uVar9 = *(uint *)(iVar6 + 0xc) & 0xfff;
            if (0xff < uVar9) {
              return;
            }
            puVar10[0x27d] = uVar8 + 1;
            *(char *)((int)puVar10 + uVar8 + 0x9f8) = (char)uVar9;
          }
          piVar1 = (int *)piVar7[1];
        }
        else {
          piVar1 = (int *)piVar7[1];
          piVar7[4] = 1;
          FUN_0012fa00(piVar7);
        }
        if (piVar1 == (int *)0x0) {
          return;
        }
        iVar6 = *piVar1;
        piVar7 = piVar1;
      } while( true );
    }
  }
  return;
}

