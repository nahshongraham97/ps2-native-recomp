"""Read-only bounded donor export using the installed Xbox Ghidra extension."""
from pathlib import Path
import json
import subprocess
import sys
import time

record = Path(__file__).resolve().parent
parent = record.parents[5]
argv = json.loads((parent / 'research/runs/xbox-donor-20261121/sound-boundary-01/command.json').read_text())['argv']
round_id = sys.argv[1]
output = record / ('XBOX-CONTEXT-'+round_id+'.json')
assert not output.exists()
assert not (record / ('XBOX-HEADLESS-'+round_id+'.json')).exists()
post = argv.index('-postScript')
argv[post+2:argv.index('-log')] = [str(output), *[hex(int(a,16)) for a in sys.argv[2:]]]
argv[argv.index('-log')+1] = str(record / ('xbox-ghidra-'+round_id+'.log'))
argv[argv.index('-scriptlog')+1] = str(record / ('xbox-scripts-'+round_id+'.log'))
start = time.monotonic()
p = subprocess.run(argv, capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
(record / ('XBOX-HEADLESS-'+round_id+'.json')).write_text(json.dumps(dict(
    argv=argv, exit=p.returncode, stdout=p.stdout, stderr=p.stderr,
    seconds=time.monotonic()-start), indent=2)+'\n', newline='\n')
print('read-only Xbox context export exit', p.returncode)
assert p.returncode == 0
assert output.is_file(), 'Ghidra process exit does not prove script succeeded'
assert 'SCRIPT ERROR' not in p.stdout+p.stderr
assert json.loads(output.read_text())['function_candidates']
