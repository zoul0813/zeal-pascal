#!/usr/bin/env python3
"""Exercise the existing full-runtime public hook contract with compact-link code."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('compact',ROOT/'tests/acceptance/test-compact-programs.py')
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
source=ROOT/'tests/native/compact-hooks.pas'
c.CASES={'FORCED':source.read_bytes()}
sys.argv=[sys.argv[0],'--case','FORCED']
c.main()
p=c.n.RUN/'compact-programs.json';report=json.loads(p.read_text())
report['source']='tests/native/compact-hooks.pas'
report['source_sha256']=hashlib.sha256(source.read_bytes()).hexdigest()
p.write_text(json.dumps(report,indent=2)+'\n')
