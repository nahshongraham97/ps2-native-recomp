
void FUN_00245cb0(undefined1 *param_1)

{
  undefined8 uVar1;
  undefined1 *puVar2;
  undefined4 *puVar3;
  int iVar4;
  
  FUN_001dba60(param_1,0,0x3c30);
  *param_1 = 1;
  uVar1 = FUN_00263e40(0);
  FUN_001dec58(param_1 + 8,uVar1);
  iVar4 = 0;
  puVar3 = &DAT_005be100;
  puVar2 = param_1;
  do {
    iVar4 = iVar4 + 8;
    *(undefined4 *)(puVar2 + 0x7c) = *puVar3;
    *(undefined4 *)(puVar2 + 0xbc) = *puVar3;
    *(undefined4 *)(puVar2 + 0x80) = puVar3[3];
    *(undefined4 *)(puVar2 + 0xc0) = puVar3[3];
    *(undefined4 *)(puVar2 + 0x84) = puVar3[6];
    *(undefined4 *)(puVar2 + 0xc4) = puVar3[6];
    *(undefined4 *)(puVar2 + 0x88) = puVar3[9];
    *(undefined4 *)(puVar2 + 200) = puVar3[9];
    *(undefined4 *)(puVar2 + 0x8c) = puVar3[0xc];
    *(undefined4 *)(puVar2 + 0xcc) = puVar3[0xc];
    *(undefined4 *)(puVar2 + 0x90) = puVar3[0xf];
    *(undefined4 *)(puVar2 + 0xd0) = puVar3[0xf];
    *(undefined4 *)(puVar2 + 0x94) = puVar3[0x12];
    *(undefined4 *)(puVar2 + 0xd4) = puVar3[0x12];
    *(undefined4 *)(puVar2 + 0x98) = puVar3[0x15];
    *(undefined4 *)(puVar2 + 0xd8) = puVar3[0x15];
    puVar3 = puVar3 + 0x18;
    puVar2 = puVar2 + 0x20;
  } while (iVar4 < 0x10);
  FUN_00514d30(param_1);
  FUN_004f3c80(param_1 + 0x930);
  FUN_001dba60(param_1 + 0x120,0xff,0xa0);
  FUN_001dba60(param_1 + 0x1c0,0xff,0x78);
  return;
}

