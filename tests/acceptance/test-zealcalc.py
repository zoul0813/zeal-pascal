#!/usr/bin/env python3
"""Compile and exercise the Pascal spreadsheet on the unmodified Zeal emulator."""
import argparse
import binascii
from concurrent.futures import ThreadPoolExecutor
import hashlib
import zlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BACKEND_BINARY = None
OVERLAYS = {}

def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

n = module('sheet_native', ROOT / 'tests/acceptance/test-native.py')


def record(row=0, col=0, previous=0, text='ZEALCALC 1'):
    encoded = text.encode('ascii')
    checksum = row + col + previous + len(encoded) + sum(encoded)
    return struct.pack('<BBH', row, col, previous) + bytes([len(encoded)]) + encoded.ljust(63, b'\0') + struct.pack('<H', checksum)


def typed_sheet(data):
    assert len(data)%70 == 0
    return struct.pack('<HH', len(data)//70, 70)+data



class CompileConsole(n.DebugConsole):
    def command(self, command, capture=False):
        if command != 'compile': return super().command(command, capture)
        call = super().command
        sym = n.symbols('zpascal')
        out, key = (sym[name] for name in ('_output','_key_read'))
        transcript = []
        for at in (out,key): call(f'bp 0x{at:x}')
        call('tap C')
        try:
            for _ in range(1000):
                call('run 2000000000')
                frame = call('regs'); pc = int(re.search(r'PC=([0-9a-fA-F]+)',frame)[1],16)
                if pc not in (out,key): continue
                observed = n.memory(call('rb 0x4000 32'))
                if bytes(observed[0x4000+i] for i in range(32)) != (ROOT/'build/zpascal.bin').read_bytes()[:32]:
                    call('s'); continue
                hl = int(re.search(r'HL=([0-9a-fA-F]+)',frame)[1],16)
                if pc == key: break
                data = n.memory(call(f'rb 0x{hl:x} 128'))
                transcript.append(bytes(data[hl+i] for i in range(128)).split(b'\0')[0].decode('ascii','replace'))
                call('s')
            else: raise RuntimeError('Native compiler did not finish')
        finally:
            for at in (out,key): call(f'bc 0x{at:x}')
        (self.directory/'compile-output.txt').write_text(''.join(transcript))
        print(''.join(transcript),flush=True)
        return ''


def compile_program(source, name, reuse=None):
    # Compile the real directory layout, exercising native include resolution
    # and its switch scoping rather than silently flattening unsupported nesting.
    text = source.read_text(encoding='ascii')
    include_files = {'include/'+p.name: p.read_bytes()
                     for p in sorted((ROOT/'examples/zealcalc/include').glob('*.pas'))}
    assert all(not re.search(rb'\{\$I(?![+-])', data, re.I) for data in include_files.values())
    assert all(len(data) <= 65534 and max(map(len, data.splitlines()), default=0) <= 127
               for data in include_files.values()), 'Include exceeds native source limits'
    assert len(text) <= 65534 and max(map(len, text.splitlines())) <= 127
    for directory in (reuse or []):
        previous = directory/('compile-'+name.lower())/'hostfs'
        candidate = previous/f'{name}.COM'
        if (candidate.is_file() and (previous/f'{name}.PAS').read_bytes() == text.encode('ascii')
                and all((previous/item).is_file() and (previous/item).read_bytes() == data
                        for item,data in include_files.items()
                        if item in re.findall(r'\{\$I\s+([^}]+)\}', text, re.I))):
            identities = ['zpascal.bin','zpascal.zpl','CRT.ZPU','ZOS.ZPU','SYSFILE.ZPU']
            assert all((previous/item).read_bytes() == (ROOT/'build'/item).read_bytes() for item in identities)
            print(f'Reusing verified compilation inputs for {name}', flush=True)
            collect_overlays(previous, name)
            return candidate.read_bytes()
    sym = n.symbols('zpascal')
    typed = lambda text: n.type_text(text, ticks=100000000)
    commands = ['run 100000000'] + n.program_launch() + ['run 100000000'] + typed('n')
    commands += typed(f'wH:/{name}.PAS\n') + ['run 500000000'] + typed('ocq')
    commands += ['compile',
                 f"rb 0x{sym['_work']:x} 140", f"rb 0x{sym['_cc']:x} 24", f"rb 0x{sym['_cc']+420:x} 32",
                 'regs', 'rb 0xfd00 256', 'tap ESC', n.ImageStop(sym['_key_read'], 'zpascal.bin'),
                 f"rb 0x{sym['_work']:x} 140", 'regs', 'quit']
    fixtures = {f'{name}.PAS': text.encode('ascii'), **include_files}
    fixtures.update({f'{unit}.ZPU': (ROOT/f'build/{unit}.ZPU').read_bytes()
                     for unit in ('CRT', 'ZOS', 'SYSFILE')})
    case = 'compile-' + name.lower()
    print(f'Compiling {source.relative_to(ROOT)}; logs {n.RUN/case}', flush=True)
    n.execute(case, commands, fixtures, timeout=1800, base_programs=('zpascal',), base_units=(), console_class=CompileConsole)
    directory = n.RUN/case
    if (directory/'input-error.json').exists():
        raise RuntimeError((directory/'input-error.json').read_text())
    candidate = directory/'hostfs'/f'{name}.COM'
    if not candidate.exists():
        memory = n.memory((directory/'console-all.txt').read_text())
        at = sym['_work']+4
        position = memory.get(at,0)+256*memory.get(at+1,0)
        log = (directory/'console-all.txt').read_text()
        if '[DBG] fd00:' in log:
            ix = int(re.findall(r'IX=([0-9a-fA-F]+)', log[:log.index('[DBG] fd00:')])[-1], 16)
            print('Compiler diagnostic:', memory.get(ix-3,0)+256*memory.get(ix-2,0), flush=True)
        print('Compiler source position:', position, repr(text[max(0,position-100):position+160]), flush=True)
        raise RuntimeError(f'Native compilation failed; inspect {directory}/console-all.txt')
    collect_overlays(directory/'hostfs', name)
    return candidate.read_bytes()


def collect_overlays(host, name):
    image = (host/(name+'.COM')).read_bytes()
    match = re.search(rb'ZP[0-9A-F]{4}/P\0', image)
    assert match, 'Missing overlay directory'
    folder = match[0][:-3].decode()
    OVERLAYS[name] = {str(p.relative_to(host)):p.read_bytes() for p in (host/folder).glob('P.*')}
    assert OVERLAYS[name]


def app_files(binary):
    return {'zealcalc.bin':binary, **OVERLAYS['ZEALCALC']}


def launch(name, binary, args='', fixtures=None, keys='', case=None):
    assert name == 'zealcalc' and not args
    commands = ['run 100000000']+n.type_text('cd H:/\n',ticks=4000000)+enter_app(name)
    commands += [ui_type(keys),'run 10000000','quit']
    data = {**app_files(binary), **(fixtures or {})}
    case = case or name
    n.execute(case,commands,data,timeout=1800,console_class=UIConsole,base_programs=(),base_units=())
    folder = n.RUN/case
    if (folder/'input-error.json').exists(): raise RuntimeError((folder/'input-error.json').read_text())
    return folder/'hostfs'


def validate_log(data):
    count,width = struct.unpack('<HH',data[:4]); assert width == 70
    assert len(data) >= 4+count*70
    data = data[4:4+count*70]
    heads = [0] * 255
    cells = {}
    for i in range(len(data)//70):
        raw = data[i*70:(i+1)*70]
        row, col, previous, length = struct.unpack('<BBHB', raw[:5])
        text = raw[5:5+length].decode('ascii')
        assert length <= 63
        assert int.from_bytes(raw[68:70], 'little') == row+col+previous+length+sum(raw[5:5+length])
        if i == 0:
            assert (row, col, previous, text) == (0, 0, 0, 'ZEALCALC 1')
        else:
            assert 1 <= row <= 254 and 1 <= col <= 26 and previous == heads[row]
            heads[row] = i
            cells[row, col] = text
    return cells


def write_png(path, pixels, width=640, height=480):
    def chunk(kind, data):
        return struct.pack('>I', len(data))+kind+data+struct.pack('>I', binascii.crc32(kind+data)&0xffffffff)
    rows = b''.join(b'\0'+pixels[y*width*3:(y+1)*width*3] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))+
                     chunk(b'IDAT', zlib.compress(rows))+chunk(b'IEND', b''))


def render_screen(vram, path):
    # Render the captured hardware character/attribute planes with the captured
    # 8x12 glyph ROM and RGB565 palette. No synthetic text/font substitutions.
    palette = []
    for i in range(16):
        color = int.from_bytes(vram[0xe00+i*2:0xe02+i*2], 'little')
        palette.append(bytes([((color>>11)&31)*255//31, ((color>>5)&63)*255//63, (color&31)*255//31]))
    pixels = bytearray()
    for y in range(480):
        for x in range(80):
            at = (y//12)*80+x
            char, attr = vram[at], vram[0x1000+at]
            bits = vram[0x3000+char*12+y%12]
            for bit in range(7, -1, -1):
                pixels.extend(palette[attr&15 if bits&(1<<bit) else attr>>4])
    write_png(path, pixels)


class ScreenConsole(n.DebugConsole):
    def trace_run(self, command, capture):
        # Observe actual UI OUT instructions without patching code or the MMU.
        for _ in range(20000):
            result = super().command(command, capture)
            frame = super().command('regs')
            pc = int(re.search(r'PC=([0-9a-fA-F]+)', frame)[1], 16)
            if pc not in self.paint_sites: return result
            observed = n.memory(super().command('rb 0x4000 32'))
            if bytes(observed[0x4000+i] for i in range(32)) == self.paint_identity:
                value = int(re.search(r'AF=([0-9a-fA-F]+)', frame)[1],16)>>8
                op = n.memory(super().command(f'rb 0x{pc:x} 2'))
                if op[pc] == 0xd3: port = op[pc+1]
                elif (op[pc],op[pc+1]) == (0xed,0x79):
                    port = int(re.search(r'BC=([0-9a-fA-F]+)', frame)[1],16)&255
                else: port = -1
                if port == 0xa1: self.paint_y = value
                elif port == 0xa2: self.paint_x = value
                elif port == 0xa0:
                    self.paint_writes.append([self.paint_y,self.paint_x,value])
                    self.paint_x += 1
            super().command('s')
        raise RuntimeError('Paint trace did not finish')

    def command(self, command, capture=False):
        if getattr(self,'paint_sites',None) and command.startswith('run '):
            return self.trace_run(command,capture)
        if not command.startswith('screen '):
            return super().command(command, capture)
        name = command.split()[1]
        image = (self.directory/'hostfs/zealcalc.bin').read_bytes()
        signatures = [bytes.fromhex('afdbf0f53e40d3f0'), bytes.fromhex('3e00dbf0f53e40d3f0')]
        domains = [(0x4000, image)]
        for payload in OVERLAYS['ZEALCALC'].values():
            count, group = struct.unpack_from('<H',payload,6)[0], struct.unpack_from('<H',payload,12)[0]
            for i in range(count):
                at = 16+16*i
                if struct.unpack_from('<H',payload,at)[0] != group: continue
                size = struct.unpack_from('<H',payload,at+4)[0]
                offset = struct.unpack_from('<I',payload,at+8)[0]
                domains.append((0x8000,payload[offset:offset+size]))
        sites = {base+i+len(signature) for base,body in domains for signature in signatures
                 for i in range(len(body)) if body.startswith(signature, i)}
        assert sites, 'No UI VRAM observation sites'
        for site in sites: super().command(f'bp 0x{site:x}')
        try:
            # Slash opens a command prompt, which clears its line after the prior
            # Paint has finished. Stop while the renderer legitimately maps VRAM.
            super().command('tap SLASH')
            for _ in range(1000):
                super().command('run 1000000000')
                frame = super().command('regs')+super().command('mmu')
                pc = int(re.search(r'PC=([0-9a-fA-F]+)', frame)[1], 16)
                if pc in sites and '0000 -> 100000' in frame: break
                super().command('s')
            else: raise RuntimeError('Renderer did not map VRAM for capture')
            data = {}
            for offset in range(0, 0x4000, 256):
                data.update(n.memory(super().command(f'rb 0x{offset:x} 256')))
            vram = bytes(data[i] for i in range(0x4000))
            (self.directory/f'{name}.vram').write_bytes(vram)
            render_screen(vram, self.directory/f'{name}.png')
            rows = [''.join(chr(vram[y*80+x] or 32) for x in range(80)) for y in range(40)]
            (self.directory/f'{name}.json').write_text(json.dumps(rows, indent=2)+'\n')
        finally:
            for site in sites: super().command(f'bc 0x{site:x}')
        super().command('s')
        return super().command('run 100000000', capture)


class UIConsole(ScreenConsole):
    def trace_start(self):
        self.ready()
        image = (self.directory/'hostfs/zealcalc.bin').read_bytes()
        self.paint_identity = image[:32]
        first,last = struct.unpack('<H',image[20:22])[0],struct.unpack('<H',image[28:30])[0]
        domains = [(first,image[first-0x4000:last-0x4000])]
        for payload in OVERLAYS['ZEALCALC'].values():
            count, group = struct.unpack_from('<H',payload,6)[0], struct.unpack_from('<H',payload,12)[0]
            for i in range(count):
                at = 16+16*i
                if struct.unpack_from('<H',payload,at)[0] != group: continue
                size = struct.unpack_from('<H',payload,at+4)[0]
                offset = struct.unpack_from('<I',payload,at+8)[0]
                domains.append((0x8000,payload[offset:offset+size]))
        self.paint_sites = {base+i for base,body in domains for i in range(len(body)-1)
                            if body[i:i+2] == b'\xed\x79' or
                            body[i] == 0xd3 and body[i+1] in (0xa0,0xa1,0xa2)}
        assert self.paint_sites
        self.paint_writes = []; self.paint_x = self.paint_y = -1
        for site in self.paint_sites: super().command(f'bp 0x{site:x}')

    def trace_stop(self, name):
        self.ready()
        for site in self.paint_sites: super().command(f'bc 0x{site:x}')
        self.paint_sites = {}
        (self.directory/f'{name}.writes.json').write_text(json.dumps(self.paint_writes)+'\n')

    def ready(self):
        key = n.kernel_symbols()['keyboard_next_key']
        fifo = n.kernel_symbols()['kb_fifo_size']
        expected = (self.directory/'hostfs/zealcalc.bin').read_bytes()[:32]
        super().command(f'bp 0x{key:x}')
        try:
            for _ in range(100):
                super().command('s')
                super().command('run 1000000000')
                frame = super().command('regs')
                if int(re.search(r'PC=([0-9a-fA-F]+)',frame)[1],16) != key: continue
                observed = n.memory(super().command('rb 0x4000 32'))
                assert bytes(observed[0x4000+i] for i in range(32)) == expected, 'UI exited or worker requested keyboard input'
                if n.memory(super().command(f'rb 0x{fifo:x} 1'))[fifo] == 0: return
            raise RuntimeError('UI did not reach its next key reader')
        finally:
            super().command(f'bc 0x{key:x}')

    def command(self, command, capture=False):
        if command == 'paint-start':
            self.trace_start(); return ''
        if command.startswith('paint-stop '):
            self.trace_stop(command.split()[1]); return ''
        if command.startswith('type '):
            for char in json.loads(command[5:]):
                self.ready()
                for sub in n.type_text(char,ticks=4000000): super().command(sub)
            return ''
        if command.startswith('key '):
            self.ready()
            super().command('tap '+command[4:])
            return super().command('run 4000000',capture)
        if command.startswith('screen '): self.ready()
        return super().command(command,capture)


class BackendConsole(n.DebugConsole):
    def command(self, command, capture=False):
        if command not in ('wait-view','wait-exit'): return super().command(command,capture)
        exit_at = n.kernel_symbols()['zos_loader_exit']
        expected = (self.directory/'hostfs/zctest.bin').read_bytes()[:32]
        super().command(f'bp 0x{exit_at:x}')
        try:
            for _ in range(100):
                super().command('s'); super().command('run 1000000000')
                frame = super().command('regs')
                if int(re.search(r'PC=([0-9a-fA-F]+)',frame)[1],16) != exit_at: continue
                observed = n.memory(super().command('rb 0x4000 32'))
                assert bytes(observed[0x4000+i] for i in range(32)) == expected, 'Backend reset or exited through another image'
                status = int(re.search(r'HL=([0-9a-fA-F]+)',frame)[1],16)>>8
                assert (status == 0) == (command == 'wait-view'), ('Unexpected backend exit',status)
                if command == 'wait-view': parse_view((self.directory/'hostfs/zc.view').read_bytes())
                return frame
            raise RuntimeError('Backend did not reach an explicit process exit')
        finally:
            super().command(f'bc 0x{exit_at:x}')


def enter_app(name):
    return n.type_text(f'H:/{name}.bin',ticks=4000000)+['tap ENTER',n.ImageStop(0x4000,name+'.bin'),'s']


def ui_type(text): return 'type '+json.dumps(text)


def seed_sheet(entries):
    data = bytearray(record()); heads = [0]*255
    for r,c,text in entries:
        index = len(data)//70; data.extend(record(r,c,heads[r],text)); heads[r] = index
    return typed_sheet(bytes(data))


def request_bytes(path='H:/engine.zvc', top=1, left=1, edit=None, generation=1):
    source = (edit[2] if edit else '').encode('ascii')
    encoded = path.encode('ascii')
    result = struct.pack('<HHB',17242,generation,len(encoded))+encoded.ljust(127,b'\0')
    result += bytes([top,left,edit[0] if edit else 0,edit[1] if edit else 0,int(edit is not None),len(source)])
    return result+source.ljust(63,b'\0')+bytes(55)


def parse_view(data):
    assert len(data) >= 128*211, ('Incomplete viewport',len(data))
    tag,generation,top,left,count = struct.unpack('<HHBBH',data[:8])
    assert tag == 17242 and generation == 1 and 1 <= count <= 4097
    assert data[8] <= 75
    result = {}
    for i in range(210):
        raw = data[128+i*128:256+i*128]
        assert raw[0] <= 63 and raw[64] <= 9 and raw[74] <= 1
        result[top+i//7,left+i%7] = {'source':raw[1:1+raw[0]].decode(),
                                    'shown':raw[65:65+raw[64]].decode().strip(), 'label':bool(raw[74])}
    return result


def pipeline(case, data, edit=None, top=1, left=1, fail=False, request=None):
    fixtures = {'zctest.bin':BACKEND_BINARY, **OVERLAYS['ZCTEST'], 'engine.zvc':data,
                'zc.req':request if request is not None else request_bytes(top=top,left=left,edit=edit),'zc.view':b''}
    commands = ['run 100000000']+n.type_text('cd H:/\n',ticks=4000000)+enter_app('zctest')
    commands += ['wait-exit' if fail else 'wait-view','run 1000000','quit']
    n.execute(case,commands,fixtures,timeout=1800,console_class=BackendConsole,base_programs=(),base_units=())
    folder = n.RUN/case
    if (folder/'input-error.json').exists(): raise RuntimeError((folder/'input-error.json').read_text())
    root = folder/'hostfs'
    if fail:
        assert (root/'engine.zvc').read_bytes() == data
        response = (root/'zc.view').read_bytes()
        assert len(response)<2 or response[:2] != struct.pack('<H',17242)
        return root,None
    response = parse_view((root/'zc.view').read_bytes())
    validate_log((root/'engine.zvc').read_bytes())
    return root,response


def check_formulas():
    entries = [(1,1,'12'),(1,2,'3.5'),(1,3,'=A1*B1'),(2,1,'=-(2+3)*4/2+11'),
               (2,2,'=@SUM(A1:B1)'),(2,3,'=a1+b1'),(3,1,'=1/0'),(3,2,'=1/0.0000000001'),
               (3,3,'=1000000000*10'),(4,1,'=A0'),(4,2,'=A255'),(4,3,'=AA1'),(4,4,'=1+'),
               (5,1,'=1..2'),(5,2,'=.'),(5,3,'=@SUM(A1:Z254)'),(5,4,'=@SUM(B2:A1)'),
               (6,1,'=@SUM(A6:A6)'),(6,2,'=@BOGUS(A1:A2)'),(6,3,'=C6'),
               (7,1,'=B7'),(7,2,'=A7'),(7,3,'=(((((((((((((((((1)))))))))))))))))'),
               (8,1,'text'),(8,2,"'99"),(8,3,'=A8+B8+Z254'),(9,1,'1E39'),
               (9,2,'=9999999999999999999999999999999999999999'),(254,26,'123.25')]
    data = seed_sheet(entries)
    _,view = pipeline('formulas',data)
    expected = {(1,3):'42.00',(2,1):'1.00',(2,2):'15.50',(2,3):'15.50',
                (3,1):'#DIV/0',(3,2):'#RANGE',(3,3):'#RANGE',(4,1):'#REF',(4,2):'#REF',
                (4,3):'#REF',(4,4):'#SYNTAX',(5,1):'#SYNTAX',(5,2):'#SYNTAX',
                (5,3):'#RANGE',(5,4):'#REF',(6,1):'#CYCLE',(6,2):'#SYNTAX',
                (6,3):'#CYCLE',(7,1):'#CYCLE',(7,2):'#CYCLE',(7,3):'#DEPTH',
                (8,2):'99',(8,3):'123.25',(9,1):'1E39',(9,2):'#SYNTAX'}
    for cell,text in expected.items(): assert view[cell]['shown'] == text, (cell,view[cell],text)
    assert view[8,1]['label'] and view[8,2]['label'] and not view[1,3]['label']
    # Off-screen row lookups and SUM, followed by a persisted tombstone.
    data = seed_sheet([(10,c,str(c)) for c in range(1,27)]+[(11,4,'=@SUM(A10:Z10)')])
    _,view = pipeline('offscreen',data); assert view[11,4]['shown']=='351.00'
    root,view = pipeline('blank',data,edit=(10,1,'')); assert view[10,1]['source']=='' and view[11,4]['shown']=='350.00'
    saved = (root/'engine.zvc').read_bytes()
    _,view = pipeline('blank-reopen',saved); assert view[11,4]['shown']=='350.00'
    root,_ = pipeline('no-op',saved,edit=(10,1,'')); assert (root/'engine.zvc').read_bytes()==saved
    # Eight dependency frames combined with unary recursion up to parser limit.
    chain = [(r,6,'=F'+str(r+1)) for r in range(1,9)]
    _,view = pipeline('dependency-depth',seed_sheet(chain)); assert view[1,6]['shown']=='#DEPTH'
    chain = chain[:7]+[(8,6,'='+'-'*8+'1')]
    _,view = pipeline('combined-depth',seed_sheet(chain)); assert view[1,6]['shown']=='1.00'
    # Deepest accepted parser/8-cell dependency combination with real disk I/O.
    io_chain = chain[:6]+[(7,6,'='+'-'*9+'Z254')]
    _,view = pipeline('combined-depth-io',seed_sheet(io_chain))
    assert view[1,6]['shown']=='0.00'
    chain[-1]=(8,6,'='+'-'*9+'1')
    _,view = pipeline('combined-depth-error',seed_sheet(chain)); assert view[1,6]['shown']=='#DEPTH'
    graph=[(30,1,'1'),(31,1,'=@SUM(A30:J30)')]
    graph += [(32,c,'=A31')for c in range(1,11)]
    graph += [(33,c,'=@SUM(A32:J32)')for c in range(1,11)]
    graph += [(34,c,'=@SUM(A33:J33)')for c in range(1,11)]+[(35,1,'=@SUM(A34:J34)')]
    _,view = pipeline('visit-limit',seed_sheet(graph),top=6); assert view[35,1]['shown']=='#LIMIT'
    return {'formula_assertions':len(expected)+3,'offscreen_lookup':True,'blank_reopen':True,'no_op':True,
            'combined_depth':True,'visit_limit':True}


def screenshots(binary):
    sheet = bytearray(record()); heads = [0]*255
    entries = [(1,1,'Product'), (1,2,'Units'), (1,3,'Price'), (1,4,'Total'),
               (2,1,'Paper'), (2,2,'12'), (2,3,'3.50'), (2,4,'=B2*C2'),
               (3,1,'Pencils'), (3,2,'8'), (3,3,'1.25'), (3,4,'=B3*C3'),
               (4,1,'Total'), (4,4,'=@SUM(D2:D3)'),
               (7,1,'Overflow'), (7,2,'1000000000'), (8,1,'Error'), (8,2,'=1/0'),
               (254,20,'On disk'), (254,26,'=D4')]
    for row, col, text in entries:
        index = len(sheet)//70
        sheet.extend(record(row,col,heads[row],text)); heads[row] = index
    commands = ['run 100000000']+n.type_text('cd H:/\n',ticks=4000000)+enter_app('zealcalc')
    commands += [ui_type('/gD4\n'),'screen sheet',ui_type('gZ254\n'),'screen scrolled',ui_type('qy')]
    commands += ['run 10000000','quit']
    n.execute('screens',commands,{**app_files(binary),
                                 'sheet.zvc':typed_sheet(bytes(sheet))},
              timeout=1800,console_class=UIConsole,base_programs=(),base_units=())
    directory = n.RUN/'screens'
    if (directory/'input-error.json').exists():
        raise RuntimeError((directory/'input-error.json').read_text())
    rows = json.loads((directory/'sheet.json').read_text())
    assert rows[3][4:14] == '    A     ', rows[3]
    assert rows[4][4:13] == 'Product  ', rows[4]
    assert rows[5][34:43].strip() == '42.00', rows[5]
    assert rows[7][34:43].strip() == '52.00', rows[7]
    assert rows[10][14:23] == '>>>>>>>>>', rows[10]
    assert rows[11][14:23].strip() == '#DIV/0', rows[11]
    edge = json.loads((directory/'scrolled.json').read_text())
    assert edge[33][0:3] == '254' and '52.00' in edge[33], edge[33]
    return directory


def check_repaint(binary):
    commands = ['run 100000000']+n.type_text('cd H:/\n',ticks=4000000)+enter_app('zealcalc')
    commands += ['paint-start',ui_type('20\n'),'paint-stop edit',
                 'paint-start',ui_type('20.000\n'),'paint-stop same-display',
                 'paint-start','key RIGHT','paint-stop move',ui_type('/qy'),
                 'run 10000000','quit']
    fixtures = {**app_files(binary),
                'sheet.zvc':seed_sheet([(1,1,'12'),(1,2,'3.5'),(1,3,'=A1*B1')])}
    n.execute('repaint',commands,fixtures,timeout=1800,console_class=UIConsole,
              base_programs=(),base_units=())
    folder = n.RUN/'repaint'
    if (folder/'input-error.json').exists(): raise RuntimeError((folder/'input-error.json').read_text())
    result = {}
    for name,expected in [('edit',{(1,1),(1,3)}),('same-display',set()),('move',{(1,1),(1,2)})]:
        writes = json.loads((folder/f'{name}.writes.json').read_text())
        grid = [(y,x) for y,x,char in writes if 4 <= y < 34 and 4 <= x < 74]
        cells = {(y-3,(x-4)//10+1) for y,x in grid}
        assert cells == expected, (name,cells,expected)
        assert len(grid) == 10*len(expected), (name,len(grid))
        assert not any(y in (3,36,37,38,39) for y,x,char in writes), (name,'static frame rewritten')
        result[name] = {'grid_characters_written':len(grid),'total_characters_written':len(writes)}
    cells = validate_log((folder/'hostfs/sheet.zvc').read_bytes())
    assert cells[1,1] == '20.000' and cells[1,3] == '=A1*B1'
    return result


def source_digest():
    digest = hashlib.sha256()
    for path in sorted((ROOT/'src').rglob('*')):
        if path.is_file():
            digest.update(str(path.relative_to(ROOT)).encode()+b'\0'+path.read_bytes())
    return digest.hexdigest()


def main():
    global BACKEND_BINARY
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', action='store_true')
    parser.add_argument('--reuse', type=Path, action='append', help='Reuse matching compilation inputs from a retained run; behavior checks still run')
    parser.add_argument('--compile-only', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    sources = [*sorted((ROOT/'examples/zealcalc').glob('*.pas')), *sorted((ROOT/'examples/zealcalc/include').glob('*.pas'))]
    sources += [Path(__file__), ROOT/'tests/native/zealcalc-backend.pas']
    frozen = {str(p.relative_to(ROOT)): n.sha(p) for p in sources}
    if args.compile_only:
        compile_program(ROOT/'examples/zealcalc/zealcalc.pas', 'ZEALCALC', args.reuse)
        return
    with ThreadPoolExecutor(max_workers=2) as workers:
        app = workers.submit(compile_program, ROOT/'examples/zealcalc/zealcalc.pas', 'ZEALCALC', args.reuse)
        backend = workers.submit(compile_program, ROOT/'tests/native/zealcalc-backend.pas', 'ZCTEST', args.reuse)
        binary = app.result(); BACKEND_BINARY = backend.result()
    reports = check_formulas()
    word = lambda at:struct.unpack_from('<H',binary,at)[0]
    members = []
    for payload in OVERLAYS['ZEALCALC'].values():
        count,group = struct.unpack_from('<H',payload,6)[0],struct.unpack_from('<H',payload,12)[0]
        for i in range(count):
            at = 16+16*i
            if struct.unpack_from('<H',payload,at)[0] != group: continue
            size = struct.unpack_from('<H',payload,at+4)[0]
            assert 0 < size <= 16384
            members.append({'member':i,'group':group,'code_bytes':size})
    reports['memory'] = {'resident_code_bytes':word(22),'resident_global_bytes':word(26),
                         'resident_total_bytes':word(22)+word(26),'checked_stack_bytes':word(32),
                         'data_bank_bytes':16384,'data_bank_used_bytes':3092,'members':sorted(members,key=lambda row:row['member'])}

    reports['repaint'] = check_repaint(binary)
    # Actual application: replace, edit-cancel, navigate, formulas, Blank, quit.
    # Shift mappings missing from the older common helper are supplied locally.
    n.SHIFTED.update({'@': '2', '>': 'PERIOD', '?': 'SLASH'})
    keys = '12\n/gB1\n3.5\n/gC1\n=A1*B1\n/gA1\n20\n'
    keys += '\n999'  # append then Escape, leaving 20 intact
    # Escape is an emulator key command rather than a printable key.
    commands = ['run 100000000']+n.type_text('cd H:/\n',ticks=4000000)+enter_app('zealcalc')
    commands += [ui_type(keys),'key ESCAPE',ui_type('/gA2\n5'),'key RIGHT',ui_type('6'),'key TAB']
    commands += [ui_type('=A2+B2\n'),'key DOWN',ui_type('=A2*B2'),'key LEFT']
    commands += [ui_type('label\n/gZ254\n'),'key RIGHT','key DOWN',ui_type('123.25\n/gB1\n/b/qy')]
    commands += ['run 10000000','quit']
    fixtures = app_files(binary)
    n.execute('ui',commands,fixtures,timeout=1800,console_class=UIConsole,base_programs=(),base_units=())
    if (n.RUN/'ui/input-error.json').exists(): raise RuntimeError((n.RUN/'ui/input-error.json').read_text())
    hostfs = n.RUN/'ui/hostfs'
    data = (hostfs/'sheet.zvc').read_bytes()
    cells = validate_log(data)
    assert cells[1, 1] == '20' and cells[1, 2] == '' and cells[1, 3] == '=A1*B1'
    assert cells[254, 26] == '123.25', cells
    assert [cells[2,c] for c in (1,2,3)] == ['5','6','=A2+B2']
    assert cells[3,3] == '=A2*B2' and cells[3,2] == 'label'
    reopened = launch('zealcalc', binary, fixtures={'sheet.zvc': data},
                      keys='/gA1\n21\n/qy', case='reopen')
    changed = validate_log((reopened/'sheet.zvc').read_bytes())
    assert changed == {**cells,(1,1):'21'}
    positive = launch('zealcalc', binary, fixtures={'sheet.zvc': data},
                      keys='1\n/qy', case='valid-control')
    assert validate_log((positive/'sheet.zvc').read_bytes())[1,1] == '1'
    # Corrupt/truncated/foreign files must remain untouched on a mutation request.
    for name,broken in [('tail',data[:-1]),('checksum',data[:-1]+bytes([data[-1]^1])),('foreign',bytes(70))]:
        pipeline(name,broken,edit=(1,1,'1'),fail=True)
    # Invalid protocol coordinates/source lengths fail before any edit.
    bad = bytearray(request_bytes(edit=(1,1,'1'))); bad[132] = 0
    pipeline('bad-request',data,fail=True,request=bytes(bad))
    # Accept the 4096th edit, refuse the next, and reopen a maximum-size log.
    full = bytearray(record())
    heads = [0]*255
    for i in range(1,4096):
        r = (i-1)%254+1
        full.extend(record(r,1,heads[r],'old')); heads[r] = i
    capped = launch('zealcalc', binary, fixtures={'sheet.zvc':typed_sheet(bytes(full))},
                    keys='new\nagain\n/qy', case='edit-limit')
    maximum = (capped/'sheet.zvc').read_bytes()
    assert len(maximum) == 286794
    assert validate_log(maximum)[1,1] == 'new'
    capped = launch('zealcalc', binary, fixtures={'sheet.zvc':maximum},
                    keys='again\n/qy', case='full-reopen')
    assert (capped/'sheet.zvc').read_bytes() == maximum
    captures = screenshots(binary)
    assert frozen == {str(p.relative_to(ROOT)): n.sha(p) for p in sources}, 'Inputs changed during validation'
    inputs = [p for p in sources if p.is_relative_to(ROOT/'examples')]
    manifest = {'format': 1, 'compiler_base_revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                'sources': {str(p.relative_to(ROOT)): n.sha(p) for p in inputs},
                'compiler_sha256': n.sha(ROOT/'build/zpascal.bin'),
                'banks_sha256': n.sha(ROOT/'build/zpascal.zpl'),
                'rom_sha256': n.sha(n.ROM), 'emulator_sha256': n.sha(n.EMULATOR),
                'binary_sha256': hashlib.sha256(binary).hexdigest(),
                'overlays': {name:hashlib.sha256(data).hexdigest() for name,data in OVERLAYS['ZEALCALC'].items()},
                'compiler_sources_sha256': source_digest(),
                'checks': reports, 'test_sources': {str(p.relative_to(ROOT)): n.sha(p) for p in sources if p.is_relative_to(ROOT/'tests')},
                'screenshots': {name: n.sha(captures/f'{name}.vram') for name in ('sheet','scrolled')},
                'units': {unit: n.sha(ROOT/f'build/{unit}.ZPU')
                                          for unit in ('CRT','ZOS','SYSFILE')}}
    (n.RUN/'zealcalc-results.json').write_text(json.dumps(manifest, indent=2)+'\n')
    destination = ROOT/'examples/zealcalc'
    if args.package:
        for name in ('sheet', 'scrolled'):
            target = destination/'screenshots'; target.mkdir(exist_ok=True)
            (target/f'{name}.png').write_bytes((captures/f'{name}.png').read_bytes())
        (destination/'zealcalc.bin').write_bytes(binary)
        for name, data in OVERLAYS['ZEALCALC'].items():
            target = destination/name; target.parent.mkdir(exist_ok=True); target.write_bytes(data)
        for obsolete in ('zcsheet.bin','zccalc.bin'):
            (destination/obsolete).unlink(missing_ok=True)
        (destination/'binary.json').write_text(json.dumps(manifest, indent=2)+'\n')
    else:
        assert (destination/'zealcalc.bin').exists(), 'Run with --package to create the verified executable'
        assert (destination/'zealcalc.bin').read_bytes() == binary, 'Run with --package to refresh executable'
        assert all((destination/name).read_bytes() == data for name,data in OVERLAYS['ZEALCALC'].items())
        previous = json.loads((destination/'binary.json').read_text())
        previous.pop('compiler_base_revision', None)
        comparison = dict(manifest); comparison.pop('compiler_base_revision')
        assert previous == comparison, 'Executable provenance differs; inspect inputs before repackaging'
    print(f'ZealCalc native checks passed; evidence: {n.RUN}', flush=True)

if __name__ == '__main__':
    main()
