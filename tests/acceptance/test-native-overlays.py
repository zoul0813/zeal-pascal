#!/usr/bin/env python3
"""Compile Pascal overlays in the native IDE and execute the saved programs."""
import importlib.util
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('native', ROOT/'tests/acceptance/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)
word = lambda data, at: struct.unpack_from('<H', data, at)[0]
typed = lambda text: n.type_text(text, ticks=2000000)


def large_program():
    source = 'program BigOvr;\nvar N,Z:Integer;\n'
    for name in ('One', 'Two', 'Three', 'Four', 'Five', 'Six'):
        source += 'overlay procedure '+name+';\nbegin\n'
        source += 'N:=N+1;\n'*600 + 'end;\n'
    source += 'begin OvrDrive(8);N:=0;Z:=0;One;Two;Three;Four;Five;Six;\n'
    source += 'if N<>3600 then N:=1 div Z end.\n'
    return source.encode('ascii')


def main():
    sym, kernel = n.symbols('zpascal'), n.kernel_symbols()
    images, companions, observations = {}, {}, {}
    cases = {'SHEETOVR': (ROOT/'examples/overlays/sheetovr.pas').read_bytes(),
             'BIGOVR': large_program()}
    cases['EDGEOVR']=b'{$A-}\nprogram EdgeOvr;\nvar N,Z:Integer;\noverlay procedure Outer(X:Integer);\nvar Local:Integer;\nfunction Twice(var Y:Integer):Integer;\nbegin Y:=Y+1;Twice:=Y*2;Exit;Twice:=0 end;\noverlay procedure Child;\nconst C:Integer=5;\nbegin C:=C+1;N:=N+X+C;N:=N+Twice(Local) end;\nbegin Local:=X;N:=N+Twice(Local);Child;Child end;\noverlay procedure Other;\nbegin N:=N+100 end;\nbegin OvrDrive(8);N:=0;Z:=0;Outer(3);Other;Outer(3);\nif N<>198 then N:=1 div Z end.\n'
    cases['WIDEOVR']=b'program WideOvr;\nvar N,Z:Integer; W:LongWord;\noverlay procedure One; var X:LongWord;\nbegin X:=70000;W:=X*3+1 end;\noverlay procedure Two; var X:LongWord;\nbegin X:=W div 7;W:=X+70000 end;\nbegin OvrDrive(8);N:=0;Z:=0;One;Two;One;Two;if W<>100000 then N:=1 div Z end.\n'
    cases['SPOOLOVR']=b'program SpoolOvr;\n{$I DATA.INC}\nvar N,Z:Integer;\n{$I FIRST.INC}\n{$I SECOND.INC}\nbegin OvrDrive(8);N:=0;Z:=0;One;Two;if N<>42 then N:=1 div Z end.\n'
    cases['INCOVR']=b'program IncOvr;\n{$I DATA.INC}\nvar N,Z:Integer;\noverlay procedure One;\n{$I BODY.INC}\noverlay procedure Two;\n{$I BODY.INC}\nbegin OvrDrive(8);N:=0;Z:=0;One;Two;if N<>42 then N:=1 div Z end.\n'
    cases['ADDROVR']=b'program AddrOvr;var P,Q,N,Z:Integer;\noverlay procedure Work;begin Q:=Addr(Work) end;\nbegin OvrDrive(8);Z:=0;P:=Addr(Work);Work;if P<>Q then N:=1 div Z end.\n'
    cases['UNITOVR']=b'program UnitOvr;\nuses Arithmetic;\nvar N,Z:Integer;\noverlay function F(X:Integer):Integer;\nbegin F:=Twice(X) end;\nbegin OvrDrive(8);Z:=0;N:=F(21);if N<>42 then N:=1 div Z end.\n'
    unit=b'unit Arithmetic;\ninterface function Twice(X:Integer):Integer;\nimplementation function Twice(X:Integer):Integer;\nbegin Twice:=X*2 end;\nend.\n'
    cases['PLAIN']=b'program Plain;\nvar N,Z:Integer;\nprocedure P;begin N:=21*2 end;\nprocedure Q;begin P end;\nbegin Z:=0;Q;if N<>42 then N:=1 div Z end.\n'
    cases['CAPOVR']=b'program CapOvr;\nvar Cells:array[1..8000]of Byte;N,Z:Integer;\noverlay procedure Fill;\nbegin Cells[1]:=11;Cells[8000]:=31 end;\nbegin OvrDrive(8);Z:=0;Fill;N:=Cells[1]+Cells[8000];\nif N<>42 then N:=1 div Z end.\n'
    cases['MISSOVR']=cases['SHEETOVR'].replace(b'program SheetOvr;',b'program SheetOvr;\n{$I-}')

    class Console(n.DebugConsole):
        def read_bytes(self, at, count):
            result=bytearray()
            for offset in range(0,count,256):
                size=min(256,count-offset)
                values=n.memory(super().command(f'rb 0x{at+offset:x} {size}'))
                result.extend(values[at+offset+i] for i in range(size))
            return bytes(result)

        def command(self, command, capture=False):
            call = super().command
            if command.startswith('@compiled-'):
                name = command.removeprefix('@compiled-')
                host = self.directory/'hostfs'
                path = host/(name+'.COM')
                assert path.exists(), f'{name}: native compilation/publication failed; see {self.directory}'
                if name in images:
                    for old,bytes_ in companions[name].items():assert (host/old).read_bytes()==bytes_
                    assert path.read_bytes()!=images[name], 'new compile must bind a fresh immutable generation'
                    observations['replacement_preserves_old_groups']=True
                images[name] = path.read_bytes()
                if name=='PLAIN':
                    companions[name]={};return ''
                packed=(ROOT/'build/overlay_runtime.packed').read_bytes()
                assert images[name][0x4000:0x4000+len(packed)]==packed
                assert packed not in images[name][0x8000:], 'bootstrap must not occupy resident page3'
                assert word(images[name],24)==0xc000
                match = re.search(rb'ZP[0-9A-F]{4}/P\0', images[name])
                assert match, 'missing compiler-bound companion directory'
                folder = match[0][:-3].decode('ascii')
                companions[name] = {str(p.relative_to(host)): p.read_bytes() for p in (host/folder).glob('P.*')}
                assert companions[name]
                assert not list(host.glob('*.$$$')) and not list((host/folder).glob('*.COM'))
                return ''
            if command=='@find':
                # F recovers a wrapped overlay PC using source alone.
                payload=next(v for p,v in companions['SHEETOVR'].items() if p.endswith('.000'))
                size=word(payload,20);offset=struct.unpack_from('<I',payload,24)[0]
                body=payload[offset:offset+size];guard=body.index(b'\xcd\x1a\x41')
                pc=word(body,guard+5)
                (self.directory/'hostfs/SHEETOVR.COM').unlink()
                for c in typed('wH:/SHEETOVR.PAS\n')+['run 100000000']+typed('of'+f'{pc:04x}'):call(c)
                out,num,key=sym['_output'],sym['_output_number'],sym['_key_read']
                for at in (out,num,key):call(f'bp 0x{at:x}')
                call('tap ENTER');found=False;line=None;want_line=False
                for _ in range(100):
                    call('run 2000000000');regs=call('regs')
                    hl=int(re.search(r'HL=([0-9a-f]+)',regs)[1],16)
                    if f'PC={out:04x} ' in regs:
                        text=self.read_bytes(hl,120).split(b'\0')[0]
                        if b'Run-time error position found' in text:found=True
                        if found and text==b' Line ':want_line=True
                    elif f'PC={num:04x} ' in regs and want_line:line=hl;want_line=False
                    elif f'PC={key:04x} ' in regs and found:break
                    call('s')
                for at in (out,num,key):call(f'bc 0x{at:x}')
                expected=cases['SHEETOVR'][:cases['SHEETOVR'].index(b'Calls:=Calls+1')].count(b'\n')+1
                assert found and line==expected,(found,line,expected)
                observations['source_lookup']={'pc':pc,'line':line,'without_saved_COM':True}
                return ''
            if not command.startswith('@finish-'):
                return call(command, capture)
            name = command.removeprefix('@finish-')
            stops=(0x40a6,0x40a9,0x4161,n.symbols('pascal_runtime')['normal_error'])
            for at in stops:call(f'bp 0x{at:x}')
            call('s')
            for _ in range(10):
                call('run 2000000000')
                registers = call('regs')
                if any(f'PC={at:04x} ' in registers for at in stops) and self.read_bytes(0x4000,32)==images[name][:32]:break
                call('s')
            assert self.read_bytes(0x4000, 32) == images[name][:32], registers
            expected_error = name=='MISSOVR'
            assert ('PC=4161 ' if expected_error else 'PC=40a6 ') in registers, registers
            if expected_error:assert 'AF=f0' in registers, registers
            # Only the24-byte gateway job/control precedes ordinary globals.
            state = self.read_bytes(0xc000+24, 12)
            boot_bytes=len((ROOT/'build/overlay_runtime.packed').read_bytes())
            if name!='PLAIN':assert self.read_bytes(0x8000,boot_bytes)==bytes(boot_bytes), 'cold bootstrap was not cleared after unpacking'
            if name == 'SHEETOVR':assert struct.unpack('<6H', state) == (20,40,60,6,120,0), state
            if name == 'BIGOVR':assert word(state,0)==3600, state
            if name == 'EDGEOVR':assert word(state,0)==198, state
            if name in ('UNITOVR','INCOVR','SPOOLOVR'):assert word(state,0)==42, state
            if name == 'CAPOVR':
                assert self.read_bytes(0xc018,1)==b'\x0b'
                assert self.read_bytes(0xc018+7999,1)==b'\x1f'
                assert word(self.read_bytes(0xc018+8000,2),0)==42
            observations[name] = {'registers':registers, 'state':state.hex(), 'mapping':call('mmu')}
            for at in stops:call(f'bc 0x{at:x}')
            call(f"bp 0x{kernel['zos_loader_exit']:x}");call('s');call('run 2000000000')
            status = call('regs')
            assert f"PC={kernel['zos_loader_exit']:04x} " in status, status
            if not expected_error:assert 'HL=000f' in status, status
            call(f"bc 0x{kernel['zos_loader_exit']:x}")
            call(f"bp 0x{kernel['keyboard_read']:x}");call('s');call('run 2000000000')
            assert f"PC={kernel['keyboard_read']:04x} " in call('regs')
            owners = self.read_bytes(kernel['_page_owners'],32)
            assert 2 not in owners, 'child pages survived process cleanup'
            call(f"bc 0x{kernel['keyboard_read']:x}")
            self.acceptance.append(registers+'\n'+status+'\n'+state.hex()+'\n')
            return ''

    commands = ['run 100000000']+n.program_launch()+['run 100000000']+typed('n')+typed('wH:/ARITH.PAS\n')+['run 100000000']+typed('ouq')+['tap C',n.ImageStop(sym['_key_read'],'zpascal.bin')]
    for name in cases:
        commands += typed('wH:/'+name+'.PAS\n')+['run 500000000']+typed('ocq')+['tap C',n.ImageStop(sym['_key_read'],'zpascal.bin'),'@compiled-'+name]
    commands += typed('wH:/SHEETOVR.PAS\n')+['run 100000000']+['tap C',n.ImageStop(sym['_key_read'],'zpascal.bin'),'@compiled-SHEETOVR','@find','quit']
    n.execute('pascal-overlays-compile', commands, {'ARITH.PAS':unit,'DATA.INC':b'const Seed=21;\n','FIRST.INC':b'overlay procedure One;begin N:=N+Seed end;\n','SECOND.INC':b'overlay procedure Two;begin N:=N+Seed end;\n','BODY.INC':b'var Local:Integer;procedure Add;begin N:=N+Local end;begin Local:=Seed;Add end;\n',**{name+'.PAS':source for name,source in cases.items()}},
              base_programs=('zpascal',), timeout=600, console_class=Console)
    failure=n.RUN/'pascal-overlays-compile/input-error.json'
    assert not failure.exists(),failure.read_text() if failure.exists() else ''
    assert set(images)==set(cases), images.keys()
    totals = {}
    for name, image in images.items():
        if name=='MISSOVR':continue
        data = companions[name]
        member_bytes = 0
        for payload in data.values():
            assert payload[:4] == b'ZBG1'
            count, group = word(payload,6), word(payload,12)
            for i in range(count):
                at = 16+16*i
                if word(payload,at)!=group:continue
                size = word(payload,at+4)
                offset = struct.unpack_from('<I',payload,at+8)[0]
                body = payload[offset:offset+size]
                assert len(body)==size and 0<size<=16384
                a=b=0
                for value in body:a=(a+value)%255;b=(b+a)%255
                assert a|(b<<8) == word(payload,at+12)
                member_bytes += size
        totals[name] = {'file_bytes':len(image),'resident_code_bytes':word(image,22),'resident_global_bytes':word(image,26),'resident_total_bytes':word(image,22)+word(image,26),'member_code_bytes':member_bytes,'aggregate_code_bytes':member_bytes+word(image,22)}
        if name=='BIGOVR':assert member_bytes+word(image,22)>65536, totals[name]
        # The unused member lives in group1: execution succeeds without its file.
        fixtures = {name+'.COM':image, **{p:v for p,v in data.items() if not(name=='SHEETOVR' and p.endswith('.001'))}}
        for repeat in range(2):
            commands = ['run 100000000']+typed('H:/'+name+'.COM')+['tap ENTER',n.ImageStop(word(image,20),name+'.COM',0),'@finish-'+name,'quit']
            n.execute(f'pascal-overlays-{name.lower()}-{repeat}',commands,fixtures,
                      base_programs=(),timeout=180,console_class=Console)
            failure=n.RUN/f'pascal-overlays-{name.lower()}-{repeat}'/'input-error.json'
            assert not failure.exists(),failure.read_text() if failure.exists() else ''
    # Missing demanded member is fatal even with I-; the uncalled group above is optional.
    name='MISSOVR'
    commands=['run 100000000']+typed('H:/'+name+'.COM')+['tap ENTER',n.ImageStop(word(images[name],20),name+'.COM',0),'@finish-'+name,'quit']
    n.execute('pascal-overlays-missing',commands,{name+'.COM':images[name]},base_programs=(),timeout=180,console_class=Console)
    failure=n.RUN/'pascal-overlays-missing/input-error.json'
    assert not failure.exists(),failure.read_text() if failure.exists() else ''
    report={'passed':True,'programs':totals,'execution':observations}
    (n.RUN/'pascal-overlays.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: native Pascal overlay compilation, shared cells, arguments/results, typed resets, unused/missing groups, >64 KiB code, repeated execution and cleanup')
    print(n.RUN)


if __name__=='__main__':main()
