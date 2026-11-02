#!/usr/bin/env python3
"""Typed/block disk I/O and file storage through native RAM/COM compilation."""
from native_chunks import SuiteChunks
import importlib.util
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def error_source_checks():
    """Exercise the real editor/compiler/diagnostic return for a file error."""
    sym=n.symbols('zpascal')
    original=b"var f:file of integer;\nbegin assign(f,'H:/NOFILE.DAT');\nreset(f)end."
    source=b' '+original
    work,build,result,key=(sym[name] for name in ('_work','_build','_ram_result','_key_read'))
    typed=lambda value:n.type_text(value,ticks=2000000)
    def probe():
        return ['regs','mmu',f"rb 0x{sym['_ide_state']:x} 1",f'rb 0x{work:x} 12',
                f'rb 0x{build:x} {n.BUILD_BYTES}',f'rb 0x{result:x} 128',f"rb 0x{sym['_ram_io_detail']:x} 2"]
    commands=['run 100000000']+n.program_launch()+['run 10000000']
    commands+=typed('nwH:/FERROR.PAS\n')+['run 100000000',f'bp 0x{key:x}',
        'tap E','run 1000000000','s','tap SPACE','run 1000000000','s']
    commands+=n.control('K')[:-1]+['run 1000000000','s','tap D','run 1000000000',
        f'bc 0x{key:x}',f"bp 0x{sym['_ram_enter']:x}",'tap R','run 60000000000',
        f"bc 0x{sym['_ram_enter']:x}",'bp 0xfd80','run 12000000000','bc 0xfd80',
        f'bp 0x{key:x}','run 1000000000']+probe()
    commands+=['s','tap ESC','run 1000000000']+probe()
    commands+=[f'bc 0x{key:x}']+n.control('K')+typed('dqn')+['quit']
    raw=n.execute('file-error-source',commands,{'FERROR.PAS':original},timeout=180)
    parts=re.split(r'\[DBG\] PC=',raw)[1:]
    if len(parts)!=2:
        return {'actual IDE file-error recovery checkpoints':False}
    diagnostic,editor=[n.memory(part) for part in parts]
    word=lambda frame,at:frame.get(at,0)|frame.get(at+1,0)<<8
    return {
        'actual IDE missing-file diagnostic retains Pascal and native errors':
            diagnostic.get(sym['_ide_state'])==3 and diagnostic.get(result+26)==3
            and diagnostic.get(result+27)==3 and diagnostic.get(result+28)==1
            and word(diagnostic,sym['_ram_io_detail'])==4,
        'file diagnostic retains exact unsaved source':
            diagnostic.get(work+1)==1 and word(diagnostic,work+2)==len(source)
            and bytes(diagnostic.get(build+15+i,0)for i in range(4))==bytes(diagnostic.get(work+8+i,0)for i in range(4)),
        'Escape selects Reset and retains dirty source after file error':
            editor.get(sym['_ide_state'])==2 and editor.get(work+1)==1
            and word(editor,work+4)==source.index(b'reset'),
        'failed file operation and unsaved edit leave disk source unchanged':
            (n.RUN/'file-error-source/hostfs/FERROR.PAS').read_bytes()==original
            and not (n.RUN/'file-error-source/hostfs/NOFILE.DAT').exists(),
    }


def main():
    chunks = SuiteChunks('files', n)
    chunks.arguments()
    target = 'zfiletest'
    sym = n.symbols(target)
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM]
    inputs += sorted((ROOT/'src').glob('*')) + sorted((ROOT/'tests/native').glob('*'))
    for name in (target,'zpascal'):
        inputs += [ROOT/f'build/{name}.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    inputs += [ROOT/'build/file_runtime.zpl',ROOT/'build/file_runtime.packed',ROOT/'build/file_gate.zpl',ROOT/'build/module_unpack.zpl']
    inputs += [ROOT/'build/diskabi.zpl']
    inputs += chunks.input_paths()
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    if not chunks.native:
        chunks.workflow_report(error_source_checks(), hashes, 'file-report.json')
        return
    commands = chunks.commands(target, sym)
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    fixtures.update({p.name.upper(): p.read_bytes() for p in (ROOT/'tests/native').glob('*.pas')})
    abi=(ROOT/'build/diskabi.zpl').read_bytes()
    body=',\n'.join(','.join(map(str,abi[i:i+20])) for i in range(0,len(abi),20))
    template=(ROOT/'tests/native/diskabi.pas.in').read_text().replace(
        '%ABI_LENGTH%',str(len(abi))).replace('%ABI_BYTES%',body)
    fixtures['DISKABI.PAS']=template.replace('%SCENARIO%','0').encode()
    fixtures['DISKFAIL.PAS']=template.replace('%SCENARIO%','1').encode()
    # The recursive local File adds176 bytes of static workspace. Relocate
    # the independent probe's three JP entries and four internal CALLs.
    cleanup_abi=bytearray(abi)
    for operand in (1,4,7,51,61,71,81):
        address=int.from_bytes(abi[operand:operand+2],'little')
        assert 0x80bd<=address<0x80bd+len(abi)
        cleanup_abi[operand:operand+2]=(address+176).to_bytes(2,'little')
    cleanup=(ROOT/'tests/native/diskcleanup.pas.in').read_text().replace(
        '%ABI_LENGTH%',str(len(abi))).replace('%ABI_BYTES%',',\n'.join(','.join(map(str,cleanup_abi[i:i+20])) for i in range(0,len(cleanup_abi),20)))
    open_file="assign(f,'H:/CLEAN.DAT');rewrite(f);i:=7;write(f,i);if dropfiles<>1 then halt;"
    heap_file="new(p);assign(p^.f,'H:/CLEAN.DAT');rewrite(p^.f);i:=7;write(p^.f,i);if dropfiles<>1 then halt;\n"
    scenarios={
        'SCOPE': ('','close(local);', 'scoped;'),
        'EXIT': ('','close(local);exit;', 'scoped;'),
        'IGNORE': ('{$I-}','close(local);', 'scoped;if ioresult<>$99 then halt;'),
        'EXITOFF': ('','{$I-}close(local);exit;{$I+}', 'scoped;if ioresult<>$99 then halt;'),
        'DISPOSE': ('','',heap_file+'close(p^.f);dispose(p);'),
        'FREE': ('','',heap_file+'close(p^.f);freemem(p,sizeof(item));'),
        'RELEASE': ('','','mark(markptr);'+heap_file+'close(p^.f);release(markptr);'),
        'FINISH': ('','',open_file+'close(f);'),
        'HALT': ('','',open_file+'close(f);halt;'),
    }
    for name,(checking,scope_exit,program) in scenarios.items():
        fixtures[name+'.PAS']=cleanup.replace('%CHECKING%',checking).replace(
            '%SCOPE_EXIT%',scope_exit).replace('%BODY%',program).encode()
    fixtures.update(chunks.fixture())
    raw = n.execute('file-component', commands, fixtures, timeout=1500)
    completion, memory, word, expected_assertions = chunks.completion(raw, sym, target)
    output = n.RUN/'file-component/hostfs/FILES.TXT'
    disk=n.RUN/'file-component/hostfs'
    checks = {
        'native completion marker': memory.get(sym['_test_done']) == 255 and word('_test_stage') == 6,
        'RAM/COM ownership, layout, aliasing and diagnostic assertions':
            word('_test_failed') == 0 and word('_test_passed') == expected_assertions,
        'file families, aggregate copies, recursion, Var, With, New and shadowing in RAM/COM':
            output.is_file() and output.read_bytes() ==
            b''.join(marker*2 for group,marker in (
                ('types',b'FILE-DECLARATIONS-OK\r\n'),('types',b'FILE-TOKENS-OK\r\n'),
                ('transfers',b'DISK-TYPED-OK\r\n'),('transfers',b'DISK-BLOCKS-OK\r\n'),
                ('lifetime',b'DISK-LIFETIME-OK\r\n'),('transfers',b'DISK-WIDE-OK\r\n'),
                ('abi',b'DISK-ABI-OK\r\n')) if chunks.wants(group)),
        'implementation and artifacts unchanged during validation':
            all(n.sha(Path(p)) == digest for p, digest in hashes.items()),
    }
    expected={'TYPED.DAT':bytes.fromhex('030002000b0063002100'),'BLOCKS.DAT':bytes(range(256)),
              'HEAPIO.DAT':bytes.fromhex('010002002a00'),'RELEASE.DAT':bytes.fromhex('010002003700'),
              'HALTIO.DAT':bytes.fromhex('000002005800'),'FAULTIO.DAT':bytes.fromhex('000002006300'),
              'ABI.DAT':bytes.fromhex('010002007b00'),
              'LOWHALT.DAT':bytes.fromhex('000002005100'),
              'LOWEXIT.DAT':bytes.fromhex('000002005200'),
              'LOWEND.DAT':bytes.fromhex('000002005300'),
              'RAMLOSS.DAT':bytes.fromhex('000002000e00'),'SAVLOSS.DAT':bytes.fromhex('000002000e00'),
              'WIDE.DAT':bytes.fromhex('01002c01')+bytes(i%251 for i in range(1,301))}
    expected.update({f'REC{i}.DAT':bytes((1,0,2,0,i,0)) for i in range(4)})
    expected['CLEAN.DAT']=bytes.fromhex('000002000700')
    owners = {'TYPED.DAT':('transfers',),'BLOCKS.DAT':('transfers',),'WIDE.DAT':('transfers',),
              'HEAPIO.DAT':('lifetime',),'RELEASE.DAT':('lifetime',),'HALTIO.DAT':('lifetime',),
              'FAULTIO.DAT':('lifetime',),'ABI.DAT':('abi',),'RAMLOSS.DAT':('abi',),'SAVLOSS.DAT':('abi',),
              'LOWHALT.DAT':('stack',),'LOWEXIT.DAT':('stack',),'LOWEND.DAT':('stack',),
              'CLEAN.DAT':('cleanup-scope','cleanup-heap')}
    owners.update({f'REC{i}.DAT':('lifetime',) for i in range(4)})
    assert set(owners) == set(expected)
    for name,data in expected.items():
        if not chunks.wants(*owners[name]): continue
        checks['exact native disk bytes '+name]=(disk/name).is_file() and (disk/name).read_bytes()==data
    if chunks.wants('errors'): checks['zero-component safety creates no disk file']=not (disk/'ZERO.DAT').exists()
    if chunks.wants('transfers'):
        image=(disk/'DISKFILES.COM').read_bytes() if (disk/'DISKFILES.COM').is_file() else b''
        checks['self-contained executable embeds checked packed module, shared decoder and gate without installed file libraries']=(
            (ROOT/'build/file_runtime.packed').read_bytes() in image and
            image.count((ROOT/'build/module_unpack.zpl').read_bytes())==1 and
            (ROOT/'build/file_gate.zpl').read_bytes() in image and
            not (disk/'file_runtime.zpl').exists() and not (disk/'file_gate.zpl').exists())
    if chunks.wants('abi'):
        abi_image=(disk/'DISKABI.COM').read_bytes() if (disk/'DISKABI.COM').exists() else b''
        checks['independently assembled alternate-register probe retained exactly']=abi_image[0x40bd:0x40bd+len(abi)]==abi
    checks.update(completion)
    if chunks.wants('ide-error'): checks.update(error_source_checks())
    checks['implementation and artifacts unchanged during validation']=all(
        n.sha(Path(p))==digest for p,digest in hashes.items())
    report = {'scope': 'Native typed/block file transfers, inline file storage, explicit Close and native terminal descriptor cleanup and RAM/self-contained COM execution; Text/devices, full paths/Rename, copied/variant adoption and full file conformance remain pending',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'case': chunks.case, 'expected_native_assertions': expected_assertions,
              'native_assertions_passed': word('_test_passed'),
              'native_assertions_failed': word('_test_failed'),
              'input_sha256': hashes, 'checks': checks}
    (n.RUN/'file-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
