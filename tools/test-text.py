#!/usr/bin/env python3
"""Disk Text through the native compiler, RAM return and standalone executables."""
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    target='ztexttest'
    sym=n.symbols(target)
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((ROOT/'src').glob('*'))+sorted((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}' for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build/file_runtime.zpl',ROOT/'build/file_gate.zpl']
    hashes={str(p):n.sha(p) for p in inputs if p.is_file()}
    assert sym['_test_stop']==0xfc00
    commands=['run 100000000']+n.program_launch(target)
    commands+=[f"bp 0x{sym['_test_stop']:x}",'run 80000000000','s','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed','_test_stage'):
        commands+=[f'rb 0x{sym[name]:x} 2']
    commands+=[f"rb 0x{sym['_test_failures']:x} 128",'quit']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes() for ext in ('bin','zpl')}
    fixtures.update({p.name.upper():p.read_bytes() for p in (ROOT/'tests/native').glob('text*.pas')})
    fixtures['CAPACITY.PAS']=(ROOT/'tests/native/capacity.pas').read_bytes()
    fixtures.update({'NUMBERS.TXT':b'-32768 255 1.5 xyz\r\nQ 42\r\n\x1a',
                     'BADNUM.TXT':b'bad 42\r\nbad\r\n256\r\nabc\r\n\x1a','NESTIN.TXT':b'A\r\n\x1a'})
    raw=n.execute('text-component',commands,fixtures,timeout=900)
    memory=n.memory(raw)
    word=lambda name:memory.get(sym[name],0)|memory.get(sym[name]+1,0)<<8
    disk=n.RUN/'text-component/hostfs'
    expected_output=b''.join(marker*2 for marker in (
        b'TEXT-DISK-OK\r\n',b'TEXT-SEEK-OK\r\n',b'TEXT-NUMBERS-OK\r\n',
        b'TEXT-FORMAT-OK\r\n',b'TEXT-RECOVERY-OK\r\n',b'TEXT-NESTING-OK\r\n',
        b'NEST=FALSE\r\nDONE\r\nTEXT-EOF-NESTING-OK\r\n',
        b'FIRST=11,TRUE\r\nSECOND=22,TRUE\r\nTHIRD=999,TRUE\r\n'
        b'LIMIT=0,0,49\r\nEMPTYCHAR=0,26,TRUE\r\nEND-NUMERIC\r\n',
        b'TEXT-ARGUMENTS-OK\r\n',b'CAPACITY-OK\r\n',b'NATIVE-PATHS-OK\r\n',b'NATIVE-RENAME-OK\r\n',b'NATIVE-RENAME-SPACE-OK\r\n'))
    checks={
        'native completion marker':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/COM ownership, stack/MMU/IFF, dirty-source and error-location assertions':
            word('_test_failed')==0 and word('_test_passed')>=100,
        'Text reads, formatting, lookahead, recovery and nested I/O in both execution modes':
            (disk/'TEXT.TXT').is_file() and (disk/'TEXT.TXT').read_bytes()==expected_output,
        'unchanged source, target artifacts and platform during validation':
            all(n.sha(Path(p))==digest for p,digest in hashes.items()),
    }
    expected={'TEXTDISK.TXT':b'ABCD\r\n  123\r\n\x1a',
              'TEXTSPACE.TXT':b'  \r\n  12  \r\n   \r\n\x1a',
              'TEXTFORMAT.TXT':b'  A  -123   TRUEZ    1.50\r\n'+b' '*254+b'x\r\n'+b'y'*255+b'\r\n\x1a',
              'NESTOUT.TXT':b'L7R\x1a','NESTSIDE.TXT':b'X\x1a','AUTO.TXT':b'AUTO\x1a',
              'LINES.TXT':b'\r\n11\r\n22\r\n\x1a','LIMIT.TXT':b'0'*30+b'1 9\r\n\x1a',
              'EMPTY.TXT':b'\x1a','TEXTARGS.TXT':b'abc 256 Q\r\n\x1a',
              'LOCALTEXT.TXT':b'LOCAL\r\n\x1a',
              'MixedName':bytes.fromhex('010002003a01'),'NoExtension':b'native\r\n\x1a',
              'RNNEW.TXT':b'rename\r\n\x1a','RNINT.NEW':bytes.fromhex('01000200c801'),
              'RNNEST.NEW':b'NEST\x1a','RNSIDE.TXT':b'SIDE\x1a','RNERROR.OLD':b'ERROR\x1a',
              'RFULL.OLD':b'FULL\r\n\x1a','RHOLD.TXT':b'hold\r\n\x1a'}
    expected.update({name:fixtures[name] for name in ('NUMBERS.TXT','BADNUM.TXT','NESTIN.TXT')})
    for name,data in expected.items():
        checks['exact disk bytes '+name]=(disk/name).is_file() and (disk/name).read_bytes()==data
    checks['Rename removes old names only after verified publication and suppresses pending transfers']=all(
        not (disk/name).exists() for name in ('RNOLD.TXT','RNINT.OLD','RNNEST.OLD','RNOPEN.NEW','RNSKIPPED','RNNOTMADE'))
    image=(disk/'TEXTDISK.COM').read_bytes() if (disk/'TEXTDISK.COM').is_file() else b''
    checks['standalone Text executable embeds the exact private component']=(
        (ROOT/'build/file_runtime.zpl').read_bytes() in image and
        (ROOT/'build/file_gate.zpl').read_bytes() in image)
    report={'scope':'Ordinary disk Text: scalar/string reads, shared formatting, CRLF/Ctrl-Z, lookahead/seek predicates, cleanup and nested I/O; preassigned/logical streams and full conformance remain pending',
            'passed':all(checks.values()),'capture_directory':str(n.RUN.relative_to(ROOT)),
            'native_assertions_passed':word('_test_passed'),'native_assertions_failed':word('_test_failed'),
            'input_sha256':hashes,'checks':checks}
    (n.RUN/'text-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    assert report['passed']


if __name__=='__main__':
    main()
