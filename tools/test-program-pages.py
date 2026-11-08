#!/usr/bin/env python3
"""Measure physical program pages through native IDE Run and standalone execution."""
import importlib.util
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)
UNIT = b'''unit Arithmetic;
interface function Twice(X:Integer):Integer;
implementation function Twice(X:Integer):Integer;begin Twice:=X*2 end;
end.'''
BASELINE_CASES = ('MINIMAL', 'INTEGER', 'REAL', 'LIBRARY')
FILE_CASES = ('BLOCKIO', 'TYPEDIO', 'TEXTIO')
STATS = b'unit Statistics;interface function Available:Integer;function Pure(X:Integer):Integer;implementation function Available:Integer;begin Available:=MaxAvail end;function Pure(X:Integer):Integer;begin Pure:=X*2 end;end.'
CASES = {
    'MINIMAL': b'begin end.',
    'INTEGER': b'var N:Integer;begin N:=21*2;if N<>42 then halt end.',
    'REAL': b'var R:Real;begin R:=1.5*4;if R<>6.0 then halt end.',
    'LIBRARY': b'uses Arithmetic;begin if Twice(21)<>42 then halt end.',
    'UNITPURE': b'uses Statistics;begin if Pure(21)<>42 then halt end.',
    'CALLS': b'procedure P;begin end;begin P end.',
    'RECURSIVE': b'{$A-}procedure P;var N:Integer;begin N:=42 end;begin P end.',
    'UNITHEAP': b'uses Statistics;begin if Available=0 then halt end.',
    'HEAP': b'var P:^Integer;begin New(P);P^:=42;if P^<>42 then halt;Dispose(P)end.',
    'HEAPVIEW': b'var N:Integer;begin N:=HeapPtr;N:=RecurPtr end.',
    'RAW': b'begin if Mem[$4000]<>$C3 then halt end.',
    'LARGE': b'var A:array[1..14000]of Byte;begin A[1]:=17;A[14000]:=42;if A[1]+A[14000]<>59 then halt end.',
    'COUNTER': b'const C:Integer=0;begin C:=C+1;if(C<1)or(C>2)then halt end.',
    'BLOCKIO': b"var F:file;B:array[0..127]of Byte;N,Z:Integer;begin Z:=0;FillChar(B,128,42);Assign(F,'H:/BLOCK.DAT');Rewrite(F);BlockWrite(F,B,1,N);Close(F);Reset(F);BlockRead(F,B,1,N);Close(F);if(N<>1)or(B[127]<>42)then N:=1 div Z end.",
    'TYPEDIO': b"var F:file of Integer;N,Z:Integer;begin Z:=0;N:=42;Assign(F,'H:/TYPED.DAT');Rewrite(F);Write(F,N);Close(F);N:=0;Reset(F);Read(F,N);Close(F);if N<>42 then N:=1 div Z end.",
    'TEXTIO': b"var F:Text;S:string[10];Z:Integer;begin Z:=0;Assign(F,'H:/TEXT.DAT');Rewrite(F);WriteLn(F,'fortytwo');Close(F);Reset(F);ReadLn(F,S);Close(F);if S<>'fortytwo'then Z:=1 div Z end.",
}

STATS = STATS.replace(b';', b';\n')
CASES = {name:source.replace(b';', b';\n') for name,source in CASES.items()}

def word(data, at):
    return int.from_bytes(data[at:at+2], 'little')


def cold_startup_probes(image, dispatch, checks, kernel):
    """Fault-inject our generated image only; kernel/ROM remain unmodified."""
    fixtures={}
    entry=word(image,dispatch+1)
    root=word(image,20)
    assert len((ROOT/'build/pascal_runtime.zpl').read_bytes())<=0x3fc0 and image[0x3fc0:0x3fc4]==bytes(4)
    for enabled in (False,True):
        for rejected in (False,True):
            data=bytearray(image)
            # The unused prefix gap is a test trampoline into the emitted stub.
            data[dispatch+1:dispatch+3]=(0x7fc0).to_bytes(2,'little')
            data[0x3fc0:0x3fc4]=bytes([0xfb if enabled else 0xf3,0xc3,entry&255,entry>>8])
            # Return2 if IFF2 no longer agrees with the injected entry state.
            # Then restore EI for the shell: OS process exit preserves IFF.
            body=bytes([0xed,0x57,0xf5,0xe1,0x7d,0xe6,4,0xfe,4 if enabled else 0,
                        0x28,5,0x21,15,2,0xfb,0xcf,0x21,15,0,0xfb,0xcf])
            data[root-0x4000:root-0x4000+len(body)]=body
            if rejected:
                needle=b'\xf1\x47\x2e\x1a\xcf'
                at=data.index(needle,entry-0x4000)
                # LDIR exhausted BC: asking pfree to release B=C=0 is rejected.
                data[at+1]=0x41
            name=f"IFF{int(enabled)}ERR{int(rejected)}"
            fixtures[name+'.COM']=bytes(data)
    for filename,data in fixtures.items():
        commands=['run 100000000']
        # A deliberately DI failing child never reaches our EI body. Observe
        # its exit/cleanup once, before the shell would wait for keyboard IRQs.
        launches=1 if filename=='IFF0ERR1.COM' else 2
        # Repeated launches also verify that cleanup restores a usable shell.
        for _ in range(launches):
            commands+=n.type_text('H:/'+filename,ticks=100000000)
            commands += ['bp 0x4000','tap ENTER','run 2000000000','regs','bc 0x4000',
                         f"bp 0x{kernel['zos_loader_exit']:x}",'s','run 2000000000','regs','mmu',
                         f"rb 0x{kernel['_page_owners']:x} 32",f"bc 0x{kernel['zos_loader_exit']:x}",
                         f"bp 0x{kernel['keyboard_read']:x}",'s','run 2000000000','regs',
                         f"rb 0x{kernel['_page_owners']:x} 32",f"rb 0x{kernel['mmu_bitmap']:x} 4",
                         f"bc 0x{kernel['keyboard_read']:x}"]
        commands+=['quit']
        raw=n.execute('program-pages-startup-'+filename.lower(),commands,{filename:data},timeout=180)
        frames=re.split(r'\[DBG\] PC=',raw)[1:]
        checks[filename+' cold checkpoints']=len(frames)==3*launches
        if len(frames)!=3*launches:continue
        snapshots=[]
        for repeat in range(launches):
            loaded,exited,cleaned=frames[repeat*3:repeat*3+3]
            expected=1 if 'ERR1' in filename else 0
            checks[filename+f' launch{repeat} startup status/IFF']=loaded.startswith('4000 ') and exited.startswith(f"{kernel['zos_loader_exit']:04x} ") and f'HL={expected:02x}0f' in exited
            owners=n.memory(exited)
            pages=sum(owners.get(kernel['_page_owners']+i)==2 for i in range(32))
            checks[filename+f' launch{repeat} owned pages']=pages==(3 if expected else 2)
            final=n.memory(cleaned)
            checks[filename+f' launch{repeat} all child pages reclaimed']=cleaned.startswith(f"{kernel['keyboard_read']:04x} ") and all(final.get(kernel['_page_owners']+i,255)!=2 for i in range(32))
            snapshots.append(bytes(final.get(kernel['mmu_bitmap']+i,255) for i in range(4)))
        if launches==2:
            checks[filename+' repeated launches preserve free-page bitmap']=snapshots[0]==snapshots[1]


def shared_guard_probe(image, checks, kernel):
    root=word(image,20)
    at=image.index(b'\xcd\x1a\x41',root-0x4000)
    expected_pc=word(image,at+5)
    commands=['run 100000000']+n.type_text('H:/GUARD.COM',ticks=100000000)
    commands+=['bp 0x4000','tap ENTER','run 2000000000','bc 0x4000',n.ImageStop(root,'GUARD.COM',0)]
    # Exhaust headroom through the debugger at the first Pascal instruction.
    # Both frontier words use the high alias; no static byte is overwritten.
    commands+=['wb 0x415c 0xf8 0xfb','wb 0x412c 0xf8 0xfb',f"bp 0x{kernel['zos_loader_exit']:x}",
               's','run 2000000000','regs','rb 0x40be 2','rb 0x8000 6',
               f"bc 0x{kernel['zos_loader_exit']:x}",f"bp 0x{kernel['keyboard_read']:x}",
               's','run 2000000000','regs',f"rb 0x{kernel['_page_owners']:x} 32",'quit']
    raw=n.execute('program-pages-guard',commands,{'GUARD.COM':image},timeout=180)
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    checks['shared Real guard checkpoints']=len(frames)==3
    if len(frames)!=3:return
    state=n.memory(frames[1])
    # At syscall entry C000 maps kernel RAM; read the preserved user-prefix
    # guard PC rather than the unmapped child's FC1E completion word.
    checks['shared Real exhausted headroom reports FF at correct PC']=frames[1].startswith(f"{kernel['zos_loader_exit']:04x} ") and 'HL=ff0f' in frames[1] and (state.get(0x40be,0)|(state.get(0x40bf,0)<<8))==expected_pc
    checks['shared Real guard preserves statics']=bytes(state.get(0x8000+i,255)for i in range(6))==bytes(6)
    final=n.memory(frames[2])
    checks['shared Real fault releases all pages']=all(final.get(kernel['_page_owners']+i,255)!=2 for i in range(32))


def main():
    sym = n.symbols('zpascal')
    kernel = n.kernel_symbols()
    private = (ROOT/'build/pascal_runtime.private').read_bytes()
    dispatch = word(private, 50)-0x4000
    def startup(image):
        return word(image, dispatch+1) if word(image,18)&2 else word(image,20)
    observed = {}
    images = {}
    checks = {}
    typed = lambda text: n.type_text(text, ticks=100000000)

    class MeasurementConsole(n.DebugConsole):
        def read(self, address, count):
            memory = n.memory(super().command(f'rb 0x{address:x} {count}'))
            return bytes(memory[address+i] for i in range(count))

        def command(self, command, capture=False):
            call = super().command
            if command.startswith('@saved-'):
                name = command.removeprefix('@saved-')
                images[name] = (self.directory/'hostfs'/f'{name}.COM').read_bytes()
                return 'Retained native saved image\n'
            if not command.startswith('@run-'):
                return super().command(command, capture)
            name = command.removeprefix('@run-')
            build = self.read(sym['_build'], n.BUILD_BYTES)
            assert build[4] == 1, name
            compile_pages = [page for page in build[:3] if page != 255]
            call('tap R')
            self.image_stop(n.ImageStop(sym['_ram_enter'], 'zpascal.bin'), capture=False)
            pages = list(self.read(sym['_ram_pages'], 3))
            cache = self.read(sym['_code_pages'], n.code_banks())
            self.image_stop(n.ImageStop(startup(images[name]), name+'.COM', 1), capture=False)
            if word(images[name], 18) & 2:
                self.image_stop(n.ImageStop(word(images[name], 20), name+'.COM', 1), capture=False)
            mapping = call('mmu')
            heap_floor = word(self.read(0x415c, 2), 0)
            expected_floor = (word(images[name], 28)+1)&~1
            if word(images[name], 18) & 2:
                expected_floor += 16384
            checks[name+' managed bounds match physical statics'] = heap_floor == expected_floor
            module = None
            if name in FILE_CASES:
                call('bp 0x4000')
                call('s'); call('run 2000000000')
                registers = call('regs')
                module = self.read(0xfe74, 1)[0]
                checks[name+' RAM file module entered'] = (
                    'PC=4000 ' in registers and self.read(0x4006, 4) == b'ZFIL')
                checks[name+' RAM file page is additional'] = 32 <= module < 64 and module not in pages
                checks[name+' RAM saved prefix handle agrees'] = self.read(0xfe76, 1)[0] == pages[0]
                module_map = dict(re.findall(r'\[DBG\] ([0-9a-f]{4}) -> ([0-9a-f]{6})', call('mmu')))
                checks[name+' RAM module mapping preserves upper windows'] = all(
                    module_map.get(slot) == f'{page*16384:06x}' for slot,page in
                    [('4000',module),('8000',pages[1]),('c000',pages[2])])
                parent_return = word(self.read(0xfc12, 2), 0)
                call('bc 0x4000'); call('s')
                self.image_stop(n.ImageStop(parent_return, 'zpascal.bin'), capture=False)
                checks[name+' RAM runtime clears module handle before parent cleanup'] = self.read(0xfe74, 1) == b'\0'
            self.image_stop(n.ImageStop(sym['_key_read'], 'zpascal.bin'), capture=False)
            result = self.read(sym['_ram_result'], 128)
            retained = list(self.read(sym['_build'], 3))
            observed[name] = {
                'file_bytes': len(images[name]),
                'code_bytes': word(images[name], 22),
                'global_bytes': word(images[name], 26),
                'static_end': word(images[name], 28),
                'image_pages_before_run': compile_pages,
                'execution_windows': pages,
                'execution_physical_pages': len(set(pages)),
                'execution_ram_bytes': len(set(pages))*16384,
                'retained_image_pages': [page for page in retained if page != 255],
                'compiler_code_pages_during_run': sum(page != 255 for page in cache),
                'root_mapping': mapping,
            }
            if module is not None:
                observed[name]['file_module_page'] = module
                observed[name]['file_call_physical_pages'] = len(set(pages+[module]))
                observed[name]['file_call_ram_bytes'] = len(set(pages+[module]))*16384
            expected_pages = 2 if name in BASELINE_CASES or name in ('COUNTER', 'UNITPURE') else 3
            checks[name+' requirement-based page count'] = len(set(pages)) == expected_pages
            checks[name+' layout flag agrees with allocation'] = bool(word(images[name], 18)&2) == (expected_pages == 2)
            checks[name+' RAM completion'] = result[26] == 1 and result[28] == 0
            if module is not None:
                checks[name+' RAM file page released before parent cleanup'] = result[81] == 0 and bytes(result[82:114]) == bytes(32)
            checks[name+' RAM mappings are owned handles'] = all(32 <= page < 64 for page in pages)
            if name == 'COUNTER':
                call('tap R')
                self.image_stop(n.ImageStop(sym['_ram_enter'], 'zpascal.bin'), capture=False)
                checks['repeated Run reuses image handles'] = list(self.read(sym['_ram_pages'], 3)) == pages
                self.image_stop(n.ImageStop(word(images[name], 20), name+'.COM', 1), capture=False)
                # The declaration jump precedes this constant.
                constant = word(images[name], 20)+3
                checks['typed constant survives repeated Run'] = self.read(constant, 2) == b'\x01\x00'
                self.image_stop(n.ImageStop(sym['_key_read'], 'zpascal.bin'), capture=False)
                repeat = self.read(sym['_ram_result'], 128)
                checks['repeated Run completes normally'] = repeat[26] == 1 and repeat[28] == 0
            return 'Measured authenticated native RAM execution\n'

    commands = ['run 100000000'] + n.program_launch() + ['run 100000000']
    commands += typed('nwH:/ARITH.PAS\nouq') + ['tap C', n.ImageStop(sym['_key_read'], 'zpascal.bin')]
    commands += typed('wH:/STATS.PAS\n') + ['tap C', n.ImageStop(sym['_key_read'], 'zpascal.bin')]
    commands += typed('ocq')
    for name in CASES:
        commands += typed('wH:/'+name+'.PAS\n') + ['tap C', n.ImageStop(sym['_key_read'], 'zpascal.bin'), '@saved-'+name]
    commands += typed('omq')
    for name in CASES:
        commands += typed('wH:/'+name+'.PAS\n') + ['tap C', n.ImageStop(sym['_key_read'], 'zpascal.bin'), '@run-'+name]
    commands += typed('q') + ['quit']
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py']
    inputs += sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))
    inputs += [ROOT/f'build/zpascal.{ext}' for ext in ('bin','zpl','map','banks.json')]
    inputs += [n.EMULATOR, n.ROM, ROOT/'build/pascal_runtime.private']
    hashes = {str(path):n.sha(path) for path in inputs if path.is_file()}
    n.execute('program-pages-ide', commands, {'ARITH.PAS':UNIT, 'STATS.PAS':STATS, **{name+'.PAS':source for name,source in CASES.items()}},
              timeout=600, console_class=MeasurementConsole)
    checks['all native images measured'] = set(observed) == set(CASES) == set(images)
    for name, image in images.items():
        commands = ['run 100000000'] + typed('H:/'+name+'.COM')
        commands += ['bp 0x4000', 'tap ENTER', 'run 2000000000', 'regs', 'mmu', 'bc 0x4000']
        commands += [n.ImageStop(startup(image), name+'.COM', 0)]
        if word(image,18)&2:
            commands += [n.ImageStop(word(image,20), name+'.COM', 0)]
        if name in FILE_CASES:
            commands += ['mmu', 'bp 0x4000', 's', 'run 2000000000', 'regs', 'mmu',
                         'rb 0x4006 4', 'rb 0xfe74 3', 'bc 0x4000']
        commands += ['mmu', f"bp 0x{kernel['zos_loader_exit']:x}", 's', 'run 2000000000', 'regs', 'mmu',
                     f"rb 0x{kernel['_page_owners']:x} 32",
                     f"bc 0x{kernel['zos_loader_exit']:x}", f"bp 0x{kernel['keyboard_read']:x}",
                     's', 'run 2000000000', 'regs', f"rb 0x{kernel['_page_owners']:x} 32", 'quit']
        raw = n.execute('program-pages-'+name.lower(), commands, {name+'.COM':image}, timeout=180)
        frames = re.split(r'\[DBG\] PC=', raw)[1:]
        expected_frames = 5 if word(image,18)&2 else 4
        if name in FILE_CASES:
            expected_frames += 1
        checks[name+' standalone checkpoints'] = len(frames) == expected_frames
        if len(frames) != expected_frames:
            continue
        state = n.memory(frames[-2])
        owners = [state.get(kernel['_page_owners']+i, 255) for i in range(32)]
        owner_pages = [i+32 for i, owner in enumerate(owners) if owner == 2]
        observed[name]['standalone_initial_mapping'] = frames[0]
        program_frame = frames[-4] if name in FILE_CASES else frames[-3]
        observed[name]['standalone_program_mapping'] = program_frame
        mappings=dict(re.findall(r'\[DBG\] ([0-9a-f]{4}) -> ([0-9a-f]{6})',program_frame))
        if name in FILE_CASES:
            module_state = n.memory(frames[-3])
            module = module_state.get(0xfe74, 0)
            windows = [int(address,16)//16384 for slot,address in re.findall(
                r'\[DBG\] ([0-9a-f]{4}) -> ([0-9a-f]{6})', program_frame)
                if slot in ('4000','8000','c000')]
            observed[name]['standalone_file_module_page'] = module
            observed[name]['standalone_file_call_physical_pages'] = len(set(windows+[module]))
            checks[name+' standalone file module entered'] = frames[-3].startswith('4000 ') and bytes(module_state.get(0x4006+i,0)for i in range(4)) == b'ZFIL'
            checks[name+' standalone file page is additional'] = 32 <= module < 64 and module not in windows and len(windows) == 3
        checks[name+' cold mapping agrees with declared layout'] = all(key in mappings for key in ('4000','8000','c000')) and mappings['4000']!=mappings['8000'] and (mappings['8000']==mappings['c000'])==bool(word(image,18)&2)
        checks[name+' prefix has independent physical backing'] = len(set(observed[name]['execution_windows'][:2])) == 2
        observed[name]['standalone_owned_pages'] = owner_pages
        observed[name]['standalone_ram_bytes'] = len(owner_pages)*16384
        checks[name+' standalone normal exit'] = frames[-2].startswith(f"{kernel['zos_loader_exit']:04x} ") and 'HL=000f' in frames[-2]
        checks[name+' standalone and RAM page counts agree'] = len(owner_pages) == observed[name]['execution_physical_pages']
        cleaned = n.memory(frames[-1])
        checks[name+' kernel releases all child owners and restores shell'] = frames[-1].startswith(f"{kernel['keyboard_read']:04x} ") and all(cleaned.get(kernel['_page_owners']+i,255)!=2 for i in range(32))
        checks[name+' saved bytes preserved'] = (n.RUN/('program-pages-'+name.lower())/'hostfs'/f'{name}.COM').read_bytes() == image
    cold_startup_probes(images['MINIMAL'],dispatch,checks,kernel)
    shared_guard_probe(images['REAL'],checks,kernel)
    checks['inputs stable'] = all(n.sha(Path(path)) == digest for path,digest in hashes.items())
    report = {'passed':all(checks.values()), 'scope':__doc__, 'checks':checks, 'programs':observed,
              'input_sha256':hashes, 'capture_directory':str(n.RUN.relative_to(ROOT)),
              'limits':['Arithmetic and file cases measured at program entry/exit; file cases additionally observe the first mapped module call. These are not exhaustive dynamic peaks.',
                        'The OS initially allocates three execution pages. Standalone ownership is measured before process cleanup.']}
    (n.RUN/'program-pages-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
