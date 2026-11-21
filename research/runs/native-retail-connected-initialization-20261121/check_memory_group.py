"""Recheck only the memory group changed by production Wave binding."""
from pathlib import Path
import importlib.util,json
F=Path(__file__).resolve().parents[3]
H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('checks',F/'research/runs/native-typed-retail-20261121/verify.py')
c=importlib.util.module_from_spec(s);s.loader.exec_module(c)
c.OUT=H/'validation-03'
c.ctypes.windll.kernel32.SetErrorMode(3)
result=json.loads((c.OUT/'verification.json').read_text())
assert result['status']=='PASS_CONNECTED_RETAIL_INITIALIZATION'
for path,pin in result['sources'].items():
    assert c.sha(path)==pin,path
c.execute('build-current-memory-group',[c.CMAKE,'--build',c.BUILD,'--target','test_native_retail_memory_owners','-j2'])
receipt=c.execute('test_native_retail_memory_owners',[c.BUILD/'test_native_retail_memory_owners.exe'])
assert 'PASS' in receipt['stdout'] and not receipt['stderr']
for path,pin in result['sources'].items():
    assert c.sha(path)==pin,path
c.save('ADDITIONAL_MEMORY_GROUP.json',dict(status='PASS_CHANGED_NATIVE_MEMORY_GROUP',
    primary_verification_sha256=c.sha(c.OUT/'verification.json'),
    executed_client=dict(target='test_native_retail_memory_owners',sha256=receipt['executable_sha256']),
    scope='Only newly changed memory-group source checked; nine existing connected client receipts retained unchanged.',
    game_executed=False,physical_endpoints_opened=False))
print('PASS_CHANGED_NATIVE_MEMORY_GROUP')
