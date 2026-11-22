
void FUN_004f3c80(undefined8 param_1)

{
  byte *pbVar1;
  
  FUN_001dba60(param_1,0,0x5d0);
  pbVar1 = (byte *)param_1;
  pbVar1[0x1fc] = 1;
  pbVar1[0x1fd] = 0;
  pbVar1[0x1fe] = 0;
  pbVar1[0x1ff] = 0;
  FUN_001dec58(pbVar1 + 0x5bc,0x647020);
  FUN_001dec58(pbVar1 + 0x2e8,0x647020);
  FUN_001dec58(pbVar1 + 0x250,0x647020);
  pbVar1[0x264] = 4;
  pbVar1[0x265] = 0;
  pbVar1[0x266] = 0;
  pbVar1[0x267] = 0;
  FUN_001dec58(pbVar1 + 700,0x647020);
  pbVar1[0x2d0] = 1;
  pbVar1[0x2d1] = 0;
  pbVar1[0x2d2] = 0;
  pbVar1[0x2d3] = 0;
  *pbVar1 = *pbVar1 & 0xfb | 4;
  pbVar1[500] = 0;
  pbVar1[0x1f5] = 0;
  pbVar1[0x1f6] = 0;
  pbVar1[0x1f7] = 0;
  pbVar1[0x268] = 2;
  pbVar1[0x269] = 0;
  pbVar1[0x26a] = 0;
  pbVar1[0x26b] = 0;
  pbVar1[0x2d4] = 1;
  pbVar1[0x2d5] = 0;
  pbVar1[0x2d6] = 0;
  pbVar1[0x2d7] = 0;
  pbVar1[0x2e0] = 10;
  pbVar1[0x2e1] = 0;
  pbVar1[0x2e2] = 0;
  pbVar1[0x2e3] = 0;
  FUN_00507f60(param_1,0);
  FUN_00507f60(param_1,1);
  FUN_00507f60(param_1,2);
  FUN_00507f60(param_1,3);
  FUN_00507f60(param_1,4);
  FUN_00507f60(param_1,5);
  return;
}

