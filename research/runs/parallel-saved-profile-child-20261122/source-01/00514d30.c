
void FUN_00514d30(long param_1)

{
  int iVar1;
  byte *pbVar2;
  uint uVar3;
  byte *pbVar4;
  
  if (param_1 != 0) {
    iVar1 = (int)param_1;
    pbVar4 = (byte *)(iVar1 + 0x1130);
    FUN_001dba60(pbVar4,0,0x29f4);
    pbVar4[0] = 0;
    pbVar4[1] = 0;
    pbVar4[2] = 0;
    pbVar4[3] = 0;
    *(undefined4 *)(iVar1 + 0x1150) = 0;
    *(undefined4 *)(iVar1 + 0x114c) = 0;
    *(undefined4 *)(iVar1 + 0x1148) = 0;
    *(undefined4 *)(iVar1 + 0x115c) = 0;
    *(undefined4 *)(iVar1 + 0x1158) = 0;
    *(undefined4 *)(iVar1 + 0x1154) = 0;
    *(undefined4 *)(iVar1 + 0x1160) = 0;
    uVar3 = 0;
    *(undefined4 *)(iVar1 + 0x1164) = 0x12;
    *(undefined4 *)(iVar1 + 0x1168) = 0xffffffff;
    *(undefined4 *)(iVar1 + 0x3b20) = 0;
    *(undefined4 *)(iVar1 + 0x1174) = 0;
    *(undefined4 *)(iVar1 + 0x1178) = 0;
    *(undefined4 *)(iVar1 + 0x117c) = 0;
    *(undefined4 *)(iVar1 + 0x1180) = 0;
    *(undefined4 *)(iVar1 + 0x1184) = 0;
    *(undefined4 *)(iVar1 + 0x1188) = 4;
    *(undefined4 *)(iVar1 + 0x118c) = 1;
    *(undefined4 *)(iVar1 + 0x1190) = 0x1b;
    *(undefined4 *)(iVar1 + 0x1194) = 0xffffffff;
    *(undefined4 *)(iVar1 + 0x1198) = 0;
    *(undefined4 *)(iVar1 + 0x119c) = 0;
    *(undefined4 *)(iVar1 + 0x11a0) = 0;
    *(undefined4 *)(iVar1 + 0x11a4) = 0x40;
    *(undefined4 *)(iVar1 + 0x11a8) = 0;
    *(undefined4 *)(iVar1 + 0x11ac) = 0x40;
    *(undefined4 *)(iVar1 + 0x11b0) = 0x40;
    *(undefined4 *)(iVar1 + 0x11b4) = 0;
    *(undefined4 *)(iVar1 + 0x11b8) = 0;
    *(undefined4 *)(iVar1 + 0x116c) = 0;
    *(undefined4 *)(iVar1 + 0x1170) = 0;
    uRam007bfc80 = 0;
    *(undefined4 *)(iVar1 + 0x3ae0) = 0;
    pbVar2 = pbVar4;
    do {
      pbVar2[0x2960] = 0;
      pbVar2[0x2961] = 0;
      pbVar2[0x2962] = 0;
      pbVar2[0x2963] = 0;
      uVar3 = uVar3 + 5;
      pbVar2[0x2964] = 0;
      pbVar2[0x2965] = 0;
      pbVar2[0x2966] = 0;
      pbVar2[0x2967] = 0;
      pbVar2[0x2968] = 0;
      pbVar2[0x2969] = 0;
      pbVar2[0x296a] = 0;
      pbVar2[0x296b] = 0;
      pbVar2[0x296c] = 0;
      pbVar2[0x296d] = 0;
      pbVar2[0x296e] = 0;
      pbVar2[0x296f] = 0;
      pbVar2[0x2970] = 0;
      pbVar2[0x2971] = 0;
      pbVar2[0x2972] = 0;
      pbVar2[0x2973] = 0;
      pbVar2 = pbVar2 + 0x14;
    } while (uVar3 < 10);
    *(undefined4 *)(iVar1 + 0x11bc) = 0;
    *(undefined4 *)(iVar1 + 0x11c0) = 0;
    *(undefined4 *)(iVar1 + 0x11c4) = 0;
    *(undefined4 *)(iVar1 + 0x11c8) = 0;
    *(undefined4 *)(iVar1 + 0x11cc) = 0;
    *(undefined4 *)(iVar1 + 0x11d0) = 0;
    *(undefined4 *)(iVar1 + 0x11d4) = 0;
    *(undefined4 *)(iVar1 + 0x11d8) = 0;
    *(undefined4 *)(iVar1 + 0x11b8) = 0;
    *(undefined4 *)(iVar1 + 0x3a7c) = 0;
    *(undefined4 *)(iVar1 + 0x3a80) = 0;
    *(undefined4 *)(iVar1 + 0x3a84) = 0;
    *(undefined4 *)(iVar1 + 0x3a88) = 0;
    *(undefined4 *)(iVar1 + 0x3a8c) = 0;
    *pbVar4 = *pbVar4 & 0xfe;
    *(undefined4 *)(iVar1 + 0x1134) = 0;
    *(undefined4 *)(iVar1 + 0x1144) = 0;
    *(undefined4 *)(iVar1 + 0x1168) = DAT_005f2550;
  }
  return;
}

