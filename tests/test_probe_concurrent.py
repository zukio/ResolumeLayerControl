"""Verify concurrent producer ordering and draining; pass the compiled executable."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
for run in range(10):
    with tempfile.TemporaryDirectory() as folder:
        log = Path(folder) / 'trace.csv'
        subprocess.run([sys.argv[1]], env=dict(os.environ, RLC_PROBE_LOG=str(log)),
                       check=True, timeout=10)
        lines = log.read_text().splitlines()
        rows = [line.split(',') for line in lines if ',concurrent,' in line]
        assert len(rows) == 3200, (run, len(rows))
        assert [int(row[1]) for row in rows] == list(range(1, 3201))
        assert all('dropped=0' in ','.join(row) for row in rows)
        assert {(row[-4], row[-3]) for row in rows} == {
            (f'a={producer}', f'b={i}') for producer in range(4) for i in range(800)}
        assert lines[-1] == 'worker_stop,dropped=0,discarded=0'
print('PASS: 10 concurrent runs; unique ordered events; zero drops; logs drained')
