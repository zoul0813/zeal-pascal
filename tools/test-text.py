#!/usr/bin/env python3
"""Disk Text through the native compiler, RAM return and standalone executables."""
from native_chunks import SuiteChunks
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    chunks = SuiteChunks('text', n)
    chunks.arguments()
    target='ztexttest'
    sym=n.symbols(target)
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+sorted((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}' for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build/file_runtime.zpl',ROOT/'build/file_runtime.packed',ROOT/'build/file_gate.zpl',ROOT/'build/module_unpack.zpl']
    inputs += chunks.input_paths()
    hashes={str(p):n.sha(p) for p in inputs if p.is_file()}
    commands = chunks.commands(target, sym)
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes() for ext in ('bin','zpl')}
    fixtures.update({p.name.upper():p.read_bytes() for p in (ROOT/'tests/native').glob('text*.pas')})
    fixtures['CAPACITY.PAS']=(ROOT/'tests/native/capacity.pas').read_bytes()
    fixtures.update({'NUMBERS.TXT':b'-32768 255 1.5 xyz\r\nQ 42\r\n\x1a',
                     'BADNUM.TXT':b'bad 42\r\nbad\r\n256\r\nabc\r\n\x1a','NESTIN.TXT':b'A\r\n\x1a'})
    fixtures.update(chunks.fixture())
    raw=n.execute('text-component',commands,fixtures,timeout=900)
    completion, memory, word, expected_assertions = chunks.completion(raw, sym, target)
    disk=n.RUN/'text-component/hostfs'
    markers = [
        ('disk-format', b'TEXT-DISK-OK\r\n'),('disk-format',b'TEXT-SEEK-OK\r\n'),
        ('numbers-recovery',b'TEXT-NUMBERS-OK\r\n'),('disk-format',b'TEXT-FORMAT-OK\r\n'),
        ('numbers-recovery',b'TEXT-RECOVERY-OK\r\n'),('nesting-arguments',b'TEXT-NESTING-OK\r\n'),
        ('nesting-arguments',b'NEST=FALSE\r\nDONE\r\nTEXT-EOF-NESTING-OK\r\n'),
        ('numbers-recovery',b'FIRST=11,TRUE\r\nSECOND=22,TRUE\r\nTHIRD=999,TRUE\r\n'
         b'LIMIT=0,0,49\r\nEMPTYCHAR=0,26,TRUE\r\nEND-NUMERIC\r\n'),
        ('nesting-arguments',b'TEXT-ARGUMENTS-OK\r\n'),('capacity-paths',b'CAPACITY-OK\r\n'),
        ('capacity-paths',b'NATIVE-PATHS-OK\r\n'),('rename',b'NATIVE-RENAME-OK\r\n'),
        ('rename',b'NATIVE-RENAME-SPACE-OK\r\n')]
    expected_output = b''.join(marker*2 for group,marker in markers if chunks.wants(group))
    checks={
        'native completion marker':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/COM ownership, stack/MMU/IFF, dirty-source and error-location assertions':
            word('_test_failed')==0 and word('_test_passed')==expected_assertions,
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
    owners = {
        'disk-format':['TEXTDISK.TXT','TEXTSPACE.TXT','TEXTFORMAT.TXT','AUTO.TXT'],
        'numbers-recovery':['LINES.TXT','LIMIT.TXT','EMPTY.TXT'],
        'nesting-arguments':['NESTOUT.TXT','NESTSIDE.TXT','TEXTARGS.TXT','LOCALTEXT.TXT'],
        'capacity-paths':['MixedName','NoExtension'],
        'rename':['RNNEW.TXT','RNINT.NEW','RNNEST.NEW','RNSIDE.TXT','RNERROR.OLD','RFULL.OLD','RHOLD.TXT']}
    owners = {name:group for group,names in owners.items() for name in names}
    assert set(owners) | {'NUMBERS.TXT','BADNUM.TXT','NESTIN.TXT'} == set(expected)
    for name,data in expected.items():
        if name in owners and not chunks.wants(owners[name]): continue
        checks['exact disk bytes '+name]=(disk/name).is_file() and (disk/name).read_bytes()==data
    if chunks.wants('rename'):
        checks['Rename removes old names only after verified publication and suppresses pending transfers']=all(
            not (disk/name).exists() for name in ('RNOLD.TXT','RNINT.OLD','RNNEST.OLD','RNOPEN.NEW','RNSKIPPED','RNNOTMADE'))
    if chunks.wants('disk-format'):
        image=(disk/'TEXTDISK.COM').read_bytes() if (disk/'TEXTDISK.COM').is_file() else b''
        checks['standalone Text executable embeds exact packed component and one shared decoder']=(
            (ROOT/'build/file_runtime.packed').read_bytes() in image and
            image.count((ROOT/'build/module_unpack.zpl').read_bytes())==1 and
            (ROOT/'build/file_gate.zpl').read_bytes() in image)
    checks.update(completion)
    report={'scope':'Ordinary disk Text: scalar/string reads, shared formatting, CRLF/Ctrl-Z, lookahead/seek predicates, cleanup and nested I/O; preassigned/logical streams and full conformance remain pending',
            'passed':all(checks.values()),'capture_directory':str(n.RUN.relative_to(ROOT)),
            'case': chunks.case, 'expected_native_assertions': expected_assertions,
              'native_assertions_passed':word('_test_passed'),'native_assertions_failed':word('_test_failed'),
            'input_sha256':hashes,'checks':checks}
    (n.RUN/'text-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    assert report['passed']


if __name__=='__main__':
    main()
