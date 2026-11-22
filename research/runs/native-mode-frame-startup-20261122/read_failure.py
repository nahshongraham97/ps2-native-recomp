"""Read the saved bounded Ghidra failure without the execute/PowerShell child bridge."""
from pathlib import Path
import gzip
record=Path(__file__).resolve().parent
print(gzip.decompress((record/"alias-functions.log.gz").read_bytes()).decode(errors="replace")[-5000:])
