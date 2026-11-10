#!/usr/bin/env python3
"""Prove product SHA removal and measure versioned-library startup via debugger."""
import importlib.util
import json
from pathlib import Path
import re
import time

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
sym=n.symbols('zpascal');binary=(ROOT/'build/zpascal.bin').read_bytes()
maps=sorted((ROOT/'build').glob('*.map'))
assert maps
for path in maps:
    text=path.read_text()
    assert not re.search(r'(?:\b_sha_|\bsha_rr|\b_sha256|\b_test_(?:source|image)_hash|sha_rotations|\[ sha256 \]|tests/host/)',text),path
rows=[]
class StartupConsole(n.DebugConsole):
    def command(self,command,capture=False):
        if command!='@startup':return super().command(command,capture)
        cmd=lambda value:super(StartupConsole,self).command(value)
        def pc():
            return int(re.search(r'PC=([0-9a-fA-F]{4})',cmd('regs'))[1],16)
        def image():
            m=n.memory(cmd('rb 0x4000 32'))
            return bytes(m.get(0x4000+i,0)for i in range(32))==binary[:32]
        def stop(label,address,already=False):
            address&=65535
            cmd(f'bp 0x{address:x}')
            for _ in range(200):
                if already and pc()==address and image():break
                cmd('run 8000000000')
                if pc()==address and image():break
                cmd('s')
            else:raise RuntimeError('qualified startup reader not reached: '+label)
            cmd(f'bc 0x{address:x}')
            result=cmd('s');cycle=int(re.search(r'cyc=(\d+)',result)[1])
            rows.append({'label':label,'cycles_after_first_instruction':cycle})
        cmd(f"bp 0x{sym['_main']:x}")
        for value in n.type_text('H:/zpascal.bin H:/zpascal.zpl\n'):cmd(value)
        stop('main',sym['_main'],True)
        stop('library_ready',sym['_code_ready'])
        stop('initial_prompt',sym['_key_read'])
        cmd('tap N')
        stop('menu_reader',sym['_key_read'])
        self.acceptance.append(cmd('regs'))
        return 'startup measured\n'
start=time.monotonic()
n.execute('no-sha-startup',['run 100000000','@startup','quit'],timeout=240,console_class=StartupConsole)
assert [r['label']for r in rows]==['main','library_ready','initial_prompt','menu_reader']
cycles=lambda i:rows[i]['cycles_after_first_instruction']
report={'passed':True,'milestones':rows,'main_to_library_cycles':cycles(1)-cycles(0),
        'main_to_initial_prompt_cycles':cycles(2)-cycles(0),
        'prompt_to_menu_cycles_including_N_delivery':cycles(3)-cycles(2),
        'no_z80_sha_symbols':True,'map_files_checked':[p.name for p in maps],'probe_wall_seconds':time.monotonic()-start,
        'input_sha256':{name:n.sha(ROOT/'build'/name)for name in ('zpascal.bin','zpascal.zpl','transfer_runtime.zpl')},
        'capture_directory':str(n.RUN.relative_to(ROOT)),
        'scope':'Native cycle measurement excludes shell executable loading and human response time; first-instruction boundary as PERF19.'}
(n.RUN/'no-sha-report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
