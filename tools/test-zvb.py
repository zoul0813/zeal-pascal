#!/usr/bin/env python3
"""Native hardware unit qualification and Pascal ZVB authoring probe."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import zlib
import os
import tempfile
ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
n=module('native',ROOT/'tools/test-native.py')
z=module('zvb_source',ROOT/'tools/zvb-source.py')
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--hardware-only",action="store_true")
    parser.add_argument("--clients",action="store_true")
    parser.add_argument("--source",nargs=2,action="append",metavar=("CASE","PATH"),help="Override a requested client source for focused example checks")
    parser.add_argument("--snake-source",type=Path,help="Compile an alternate Snake source and adjacent includes")
    parser.add_argument("--disk-snake",action="store_true",help="Compile Snake through the saved-program disk workspace")
    parser.add_argument("--skip-hardware",action="store_true")
    parser.add_argument("--prebuilt",type=Path,help="Test clients against an already authored ZVB.ZPU")
    parser.add_argument("--case",action="append",choices=['USES','ORDER','SMOKE','CRC','DMA','GFX','PACK','OPTIONS','PAGE','SOUND','ZVBCHECK','SPRITES','SNAKE','FIRSTBAR','CRC32'])
    parser.add_argument("--output-library",type=Path,help="Install a freshly authored, verified ZPU at this path")
    parser.add_argument("--emulator",type=Path,help="Use an installed native emulator")
    args=parser.parse_args()
    if args.emulator:n.EMULATOR=args.emulator
    if args.output_library and (args.hardware_only or args.prebuilt):
        parser.error("output-library requires fresh Pascal unit authoring")
    if args.hardware_only and (args.clients or args.case or args.skip_hardware or args.prebuilt):parser.error("hardware-only excludes client fixtures")
    target='zzvbtest';symbols=n.symbols(target)
    fixtures={target+'.'+ext:(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    if args.prebuilt:
        fixtures['ZVB.ZPU']=args.prebuilt.read_bytes();fixtures['PREBUILT.RUN']=b'1'
    elif not args.hardware_only:fixtures['ZVB.PAS']=z.source().encode()
    if args.skip_hardware:fixtures['SKIPHW.RUN']=b'1'
    cases=args.case or (['USES','ORDER','SMOKE','CRC','DMA','GFX','PACK','OPTIONS','PAGE','SOUND','ZVBCHECK','SPRITES','SNAKE','FIRSTBAR','CRC32']if args.clients else [])
    overrides={case:Path(path)for case,path in args.source or []}
    if len(overrides)!=len(args.source or []):parser.error("duplicate source override")
    if args.snake_source:
        if 'SNAKE' in overrides:parser.error("duplicate Snake source override")
        overrides['SNAKE']=args.snake_source
    if set(overrides)-set(cases):parser.error("source overrides require their requested client cases")
    if args.disk_snake:
        if 'SNAKE' not in cases:parser.error("disk-snake requires the Snake client")
        fixtures['DISKSNAKE.RUN']=b'1'
    if cases:
        fixtures['CLIENTS.RUN']=b'1'
        fixtures['CRT.ZPU']=(ROOT/'build/CRT.ZPU').read_bytes()
        for name in cases:
            path=ROOT/('tests/native/zvb'if name not in ('ZVBCHECK','SPRITES','SNAKE','CRC32')else 'examples/zvb')/(name+'.PAS')
            path=overrides.get(name,path)
            fixtures[name+'.PAS']=path.read_bytes()
            if name=='FIRSTBAR' and fixtures[name+'.PAS']!=z.first_bar_source().encode():
                parser.error('FirstBar fixture is stale; run tools/zvb-source.py --demos')
        fixtures.update({p.name:p.read_bytes()for p in(ROOT/'examples/zvb').glob('*.INC')})
        if 'SMOKE' in cases:
            fixtures['HWVALUES.INC']=(ROOT/'tests/native/zvb/HWVALUES.INC').read_bytes()
        if 'PACK' in cases:
            fixtures['ZVBSTATE.INC']=(ROOT/'libraries/ZVBDECODE.INC').read_text().split('procedure DecodeEncoded',1)[0].encode()
        for path in overrides.values():
            fixtures.update({p.name:p.read_bytes()for p in path.parent.glob('*.INC')})
        if 'CRC32' in cases:
            fixtures.update({'EMPTY.DAT':b'', 'DIGITS.DAT':b'123456789',
                             'LARGE.DAT':bytes(range(256))*17+b'finish'})
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{symbols['_test_stop']:x}",'run 120000000000'if cases else 'run 60000000000','s','regs','rb 0x4000 32']
    fields=('_test_done','_test_passed','_test_failed','_test_index','_test_stage','_test_unit_ok','_test_unit_error','_test_unit_position','_test_unit_code','_test_unit_types','_test_unit_attempted','_test_unit_maps','_test_unit_relocations','_test_client_count','_test_stack_low','_test_heap_base')
    commands += [f'rb 0x{symbols[name]:x} 2'for name in fields]
    commands += [f"rb 0x{symbols['_test_clients']:x} 192",f"rb 0x{symbols['_test_failures']:x} 128",f"rb 0x{symbols['_test_diagnostics']:x} 256",'quit']
    fixture_hashes={name:hashlib.sha256(data).hexdigest() for name,data in fixtures.items()}
    inputs=[ROOT/'CMakeLists.txt']+sorted(p for p in (ROOT/'src').iterdir() if p.is_file())
    inputs += [ROOT/'build'/name for name in ('zpascal.bin','zpascal.zpl','zpascal.map',
               'zpascal.banks.json','pascal_runtime.zpl','pascal_runtime.private',
               'pascal_runtime.profile','runtime-layout.json')]
    inputs += sorted(p for p in (ROOT/'libraries').iterdir() if p.is_file())
    inputs += [ROOT/'tools/zvb-source.py', ROOT/'tools/test-zvb.py']
    input_hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    standalone=[]
    class SavedClients(n.DebugConsole):
        def command(self,command,capture=False):
            if command!='@clients-complete':return super().command(command,capture)
            call=lambda value:super(SavedClients,self).command(value)
            entry=symbols['_exec'];stop=symbols['_test_stop'];return_at=None;held=[];current_client=None
            def supervisor():
                state=n.memory(call('rb 0x4000 32'))
                return bytes(state.get(0x4000+i,0)for i in range(32))==fixtures[target+'.bin'][:32]
            def restore():
                while held:
                    original,backup=held[-1]
                    if original.exists():raise RuntimeError('saved client recreated a held library/source')
                    backup.rename(original);held.pop()
            call(f'bp 0x{entry:x}')
            try:
                for _ in range(256):
                    call('run 20000000000');registers=call('regs')
                    if f'PC={entry:04x} 'in registers and return_at is None and supervisor():
                        state=n.memory(call(f"rb 0x{symbols['_test_stage']:x} 2"))
                        stage=state[symbols['_test_stage']]|(state[symbols['_test_stage']+1]<<8)
                        if not 10<=stage<10+len(cases):
                            call('s');continue
                        current_client=stage-10
                        sp=int(re.search(r'SP=([0-9a-fA-F]+)',registers)[1],16)
                        state=n.memory(call(f'rb 0x{sp:x} 2'));return_at=state[sp]|(state[sp+1]<<8)
                        for path in(self.directory/'hostfs').iterdir():
                            if path.suffix.upper()in('.ZPU','.PAS','.INC'):
                                backup=path.with_name(path.name+'.HOLD')
                                if backup.exists():raise RuntimeError('held source already exists')
                                path.rename(backup);held.append((path,backup))
                        call(f'bc 0x{entry:x}');call(f'bp 0x{return_at:x}')
                    elif return_at is not None and f'PC={return_at:04x} 'in registers and supervisor():
                        restore();standalone.append(current_client)
                        call(f'bc 0x{return_at:x}');return_at=None;call(f'bp 0x{entry:x}')
                    elif f'PC={stop:04x} 'in registers and supervisor():
                        state=n.memory(call(f"rb 0x{symbols['_test_done']:x} 1"))
                        if state.get(symbols['_test_done'])==255:return 'saved clients completed without libraries or sources\n'
                    call('s')
                raise RuntimeError('client completion not reached')
            finally:restore()
    if cases:
        commands[commands.index('run 120000000000')]='@clients-complete'
    raw=n.execute('zvb',commands,fixtures,timeout=3600 if cases else 1200,
                  console_class=SavedClients if cases else None)
    frame=re.split(r'\[DBG\] PC=',raw)[-1];state=n.memory(frame)
    word=lambda name:state.get(symbols[name],0)|(state.get(symbols[name]+1,0)<<8)
    byte=lambda name:state.get(symbols[name],0)
    checks={'authenticated completion':frame.startswith('fc01 ')and byte('_test_done')==255 and bytes(state.get(0x4000+i,0)for i in range(32))==fixtures[target+'.bin'][:32],
            'native assertions pass':word('_test_passed')==word('_test_index')and word('_test_passed')>0 and word('_test_failed')==0}
    if args.hardware_only:
        # 11 author fixtures (6 checks), two clients (9), five lifecycle
        # checks, plus 13 closes checking every configured compiler bank slot.
        bank_limit=int(re.search(r'^#define CODE_BANK_LIMIT (\d+)',
            (ROOT/'src/library.h').read_text(),re.M)[1])
        expected_assertions=11*6+2*9+5+13*(1+bank_limit)
        checks['exact focused assertion count']=word('_test_index')==expected_assertions
        checks['library probe skipped']=byte('_test_unit_attempted')==0
    elif args.prebuilt:checks['prebuilt library used']=byte('_test_unit_attempted')==0
    else:checks['Pascal ZVB compiled']=byte('_test_unit_attempted')==1 and byte('_test_unit_ok')==1 and byte('_test_unit_error')==0
    if cases:
        checks['requested clients compiled and executed']=word('_test_client_count')==len(cases)
        checks['saved clients run without ZPU PAS or INC files']=sorted(standalone)==list(range(len(cases)))
    if 'CRC32' in cases:
        output=(n.RUN/'zvb/hostfs/CRCOUT.TXT')
        expected=''.join(f'{zlib.crc32(fixtures[name]):08X}    H:/{name}\n'
                         for name in ('EMPTY.DAT','DIGITS.DAT','LARGE.DAT'))
        checks['file utility CRC32 golden output']=output.exists() and output.read_text()==expected
    if 'SNAKE' in cases:
        output=n.RUN/'zvb/hostfs/SNAKEOUT.TXT'
        checks['Snake completes 180 demo frames']=output.exists() and 'SNAKE-DEMO-OK' in output.read_text()
    metrics=[]
    names=['USES','ORDER','SMOKE','CRC','DMA','GFX','PACK','OPTIONS','PAGE','SOUND','ZVBCHECK','SPRITES','SNAKE','FIRSTBAR','CRC32']
    for index,name in enumerate(name for name in names if name in cases):
        values=[]
        for field in range(6):
            at=symbols['_test_clients']+12*index+2*field
            values.append(state.get(at,0)|(state.get(at+1,0)<<8))
        metrics.append(dict(zip(('flags','code_bytes','globals_bytes','file_bytes','compile_error','position'),values),name=name))
    if 'USES' in cases and 'ORDER' in cases:
        directory=n.RUN/'zvb/hostfs'
        checks['named/literal ports and reversed uses order produce identical output']=(directory/'USES.COM').exists() and (directory/'ORDER.COM').exists() and (directory/'USES.COM').read_bytes()==(directory/'ORDER.COM').read_bytes()
    checks['compiler and runtime inputs unchanged']=all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==value for p,value in input_hashes.items())
    report={'standalone_clients':standalone,'native_stack_margin':word('_test_stack_low')-word('_test_heap_base'),'clients':metrics,'scope':'hardware unit foundation'if args.hardware_only else __doc__,'passed':all(checks.values()),'checks':checks,'assertions':word('_test_passed'),'failed':word('_test_failed'),
            'failure_indices':[state.get(symbols['_test_failures']+2*i,0)|(state.get(symbols['_test_failures']+2*i+1,0)<<8)for i in range(min(word('_test_failed'),64))],
            'diagnostics':[state.get(symbols['_test_diagnostics']+2*i,0)|(state.get(symbols['_test_diagnostics']+2*i+1,0)<<8)for i in range(min(2*word('_test_failed'),128))],
            'zvb':{'compiled':byte('_test_unit_ok'),'error':byte('_test_unit_error'),'position':word('_test_unit_position'),'code_bytes':word('_test_unit_code'),'types':word('_test_unit_types'),'maps':word('_test_unit_maps'),'code_relocations':word('_test_unit_relocations')},
            'emulator':str(n.EMULATOR),'capture_directory':str(n.RUN.relative_to(ROOT))}
    report['input_sha256']=input_hashes
    report['fixture_sha256']=fixture_hashes
    directory=n.RUN/'zvb/hostfs'
    report['program_sha256']={case:hashlib.sha256((directory/(case+'.COM')).read_bytes()).hexdigest()
                              for case in cases if (directory/(case+'.COM')).exists()}
    if (directory/'ZVB.ZPU').exists():
        report['library_sha256']=hashlib.sha256((directory/'ZVB.ZPU').read_bytes()).hexdigest()
    if args.output_library and report['passed']:
        destination=args.output_library.resolve();destination.parent.mkdir(parents=True,exist_ok=True)
        payload=(n.RUN/'zvb/hostfs/ZVB.ZPU').read_bytes()
        with tempfile.NamedTemporaryFile(dir=destination.parent,delete=False) as stream:
            temporary=Path(stream.name)
            try:
                stream.write(payload);stream.flush();os.fsync(stream.fileno())
                stream.close();os.replace(temporary,destination)
            finally:
                temporary.unlink(missing_ok=True)
        report['installed_library']=str(destination)
    if overrides:report['source_overrides']={case:str(path)for case,path in overrides.items()}
    (n.RUN/'zvb-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
