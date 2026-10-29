#!/usr/bin/env python3
"""Native Execute success and replacement-owned startup resource retirement.

Images use the real runtime and independently authored fixture bodies, not
compiler output. Production Pascal syntax/file cleanup remain separate gates.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct

ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result

class Code:
    """Bounded fixture byte writer; production uses the ZDE-assembled template."""
    def __init__(self,origin):self.origin=origin;self.data=bytearray();self.labels={};self.patches=[]
    def emit(self,*bytes):self.data.extend(bytes)
    def word(self,value):self.data.extend(struct.pack('<H',value))
    def mark(self,name):self.labels[name]=self.origin+len(self.data)
    def jump(self,name,opcode=0xc3):
        self.emit(opcode);self.patches.append((len(self.data),name));self.word(0)
    def finish(self):
        for at,name in self.patches:struct.pack_into('<H',self.data,at,self.labels[name])
        return bytes(self.data)

def body(origin,warm,tail=b''):
    c=Code(origin)
    c.emit(0x3a,0,0xfc,0xb7);c.jump('fail',0xc2) # consume/clear context before user code
    if tail is not None:
        c.emit(0x3a,0,0xff,0xfe,len(tail));c.jump('fail',0xc2)
        for i,value in enumerate(tail):
            c.emit(0x3a);c.word(0xff01+i);c.emit(0xfe,value);c.jump('fail',0xc2)
    if warm:
        c.emit(0xfd,0x21,0,0xe0,0x0e,4);c.mark('allocate')
        c.emit(0xc5,0x2e,25,0xcf,0xb7);c.jump('fail_pop',0xc2)
        c.emit(0x78,0xc1,0xfd,0x77,0,0xfd,0x23,0x57,0x21,0x2a,0xfc,0x06,4)
        c.mark('member');c.emit(0x7e,0xba);c.jump('found',0xca)
        c.emit(0x23,0x05);c.jump('member',0xc2);c.jump('fail')
        c.mark('found');c.emit(0x0d);c.jump('allocate',0xc2)
        c.emit(0xdd,0x21,0,0xe0,0x0e,4);c.mark('free')
        c.emit(0xc5,0xdd,0x46,0,0x2e,26,0xcf,0xc1,0xb7);c.jump('fail',0xc2)
        c.emit(0xdd,0x23,0x0d);c.jump('free',0xc2)
    for byte in (b'EXEC-HANDOFF-OK\r\n'if warm else b'EXEC-COLD-OK\r\n'):
        c.emit(0x3e,byte,0xcd,0xac,0x40)
    c.emit(0xc3,0xa6,0x40)
    c.mark('fail_pop');c.emit(0xc1)
    c.mark('fail');c.emit(0x21,15,1,0xcf,0x18,0xfe)
    return c.finish()

def bootstrap(origin,entry,variant):
    c=Code(origin);c.emit(0x31,0,0xfc)
    if variant in (13,14,15):c.emit(0x1a,0x32,0x70,0xfc)
    c.emit(0x21,0,0xfc,0x11,1,0xfc,0x01,71,0,0x36,0,0xed,0xb0)
    c.emit(0x21,0x5a,0x45,0x22,0,0xfc,0x21,0x58,0x31,0x22,2,0xfc,0x3e,4,0x32,4,0xfc)
    for value,port,offset in ((0x40,0xf1,5),(0x80,0xf2,6),(0xc0,0xf3,7)):
        c.emit(0x3e,value,0xdb,port,0x32,offset,0xfc)
    c.emit(0x2a,10,0x40,0x22,8,0xfc,0x21,36,0x40,0x11,10,0xfc,0x01,32,0,0xed,0xb0)
    if variant<15:
        c.emit(0xdd,0x21,42,0xfc,0x0e,4);c.mark('allocate')
        c.emit(0xc5,0x2e,25,0xcf,0xb7);c.jump('fail_pop',0xc2)
        c.emit(0x78,0xc1,0xdd,0x77,0,0xdd,0x23,0x0d);c.jump('allocate',0xc2)
    if variant in (2,3):c.emit(0x3e,3 if variant==2 else 30,0x32,4,0xfc)
    elif variant==4:c.emit(0x2a,8,0xfc,0x23,0x22,8,0xfc)
    elif variant in (5,6):
        offset=10 if variant==5 else 5;c.emit(0x3a,offset,0xfc,0xee,1,0x32,offset,0xfc)
    elif variant in (7,8):c.emit(0x3e,31 if variant==7 else 64,0x32,43,0xfc)
    elif variant in (9,10,11,12):
        c.emit(0x3a,{9:5,10:6,11:7,12:42}[variant],0xfc,0x32,43,0xfc)
    elif variant in (13,14):c.emit(0x3a,0x70,0xfc,0x32,42 if variant==13 else 43,0xfc)
    elif variant>=15:
        c.emit(0x3e,3,0x32,4,0xfc)
        if variant==15:c.emit(0x3a,0x70,0xfc)
        elif variant<=18:c.emit(0x3a,variant-11,0xfc)
        else:c.emit(0x3e,{19:0,20:31,21:64,22:255}[variant])
        c.emit(0x32,42,0xfc)
    c.emit(0x21,0,0xfc,0x06,72,0xaf);c.mark('checksum')
    c.emit(0xae,0x23,0x05);c.jump('checksum',0xc2);c.emit(0x32,71,0xfc)
    if variant==1 or variant>=15:c.emit(0x3a,71,0xfc,0xee,1,0x32,71,0xfc)
    c.emit(0xc3);c.word(entry)
    c.mark('fail_pop');c.emit(0xc1,0x21,15,1,0xcf,0x18,0xfe)
    return c.finish()

def image(f,normal=0x8003,warm=True,variant=None,tail=b''):
    entry=(ROOT/'build/execute_entry.zpl').read_bytes()
    body_at=normal+len(entry);code=body(body_at,warm,tail)
    boot_at=body_at+len(code);boot=bootstrap(boot_at,normal,variant)if variant is not None else b''
    b=bytearray((ROOT/'build/pascal_runtime.zpl').read_bytes());assert len(b)<=16384
    b.extend(bytes(16384-len(b))) # zero-initialized trailing runtime workspace
    b.extend(bytes(boot_at+len(boot)-0x8000))
    b[16384]=0xc3;f.put(b,16385,body_at)
    b[normal-0x4000:body_at-0x4000]=entry;b[body_at-0x4000:boot_at-0x4000]=code
    b[boot_at-0x4000:]=boot
    start=boot_at if variant is not None else normal
    for at,value in ((1,start),(10,len(b)),(12,start),(20,0x8000),(22,len(b)-16384),
                     (26,0),(28,0x4000+len(b))):f.put(b,at,value)
    b[68:100]=hashlib.sha256(b'independently authored Execute fixture').digest()
    b[100:132]=hashlib.sha256(b'no source-map acceptance claimed').digest()
    f.seal_executable(b);return bytes(b),body_at

def main():
    n=module('native',ROOT/'tools/test-native.py');f=module('loader',ROOT/'tools/test-chain-loader.py')
    parent='zexparent';sym=n.symbols(parent);template=n.symbols('execute_entry')
    parent_bytes=(ROOT/f'build/{parent}.bin').read_bytes()
    caller=(ROOT/'build/zexecuteprobe.bin').read_bytes();assert len(parent_bytes)<=16384
    inputs=[Path(__file__),ROOT/'tools/test-native.py',ROOT/'tools/test-chain-loader.py',ROOT/'CMakeLists.txt',n.EMULATOR,n.ROM]
    inputs+=list((ROOT/'src').glob('*'))+list((ROOT/'tests/native').glob('execute-*'))
    inputs+=[ROOT/f'build/{target}.{ext}'for target in (parent,'zexecuteprobe','execute_entry')for ext in ('bin','map')if (ROOT/f'build/{target}.{ext}').exists()]
    inputs += [ROOT/'build/execute_entry.zpl',ROOT/'build/pascal_runtime.zpl']
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()};cases={}
    configurations=[('override',None,0x8003,0),('cold',None,0x8003,1),('cold-cross',None,0xbfff,1)]
    configurations += [('record-'+str(i),i,0x8003,2 if i==14 else 0)for i in range(15)if i!=13]
    configurations += [('cold-foreign',13,0x8003,2)]
    configurations += [('cold-stale-'+str(i),i,0x8003,2 if i==15 else 0)for i in range(15,23)]
    for label,variant,normal,arguments in configurations:
        cold=label.startswith('cold');expected=0 if cold or variant in (None,0)else 1
        target,body_at=image(f,normal,not cold,variant,b'HELLO'if cold and variant is None else None if variant in (13,14,15)else b'')
        child='zexecuteprobe.bin'if label=='override'else 'TARGET.COM'
        fixtures={parent+'.bin':parent_bytes,'zexecuteprobe.bin':caller,'TARGET.COM':target,
                  'x-max.COM':target,'CASE.TXT':b'H:/TARGET.COM',
                  'PARENT.TXT':bytes([expected,arguments])+('H:/'+child).encode()}
        release=normal+template['execute_entry_release']-0x8000
        plan_valid=not cold and variant in (None,0,14)
        commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
        commands+=['bp 0x4000','tap ENTER','run 1000000000','bc 0x4000',
                   f'bp 0x{release:x}',f"bp 0x{sym['_execute_parent_stop']:x}",'s','run 10000000000','regs','mmu','rb 0xfc00 72']
        if plan_valid:
            commands += [f'bc 0x{release:x}','s','run 1000000000','regs','mmu']
        for name in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{sym[name]:x} 2']
        commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
        raw=n.execute('execute-handoff-'+label,commands,fixtures,timeout=600)
        frames=re.split(r'\[DBG\] PC=',raw)[1:];state=n.memory(frames[-1])if frames else {}
        word=lambda at:state.get(at,0)|(state.get(at+1,0)<<8)
        directory=n.RUN/('execute-handoff-'+label)
        checks={'expected native checkpoints reached':len(frames)==(2 if plan_valid else 1),
                'supervisor observes expected replacement status, foreign-page preservation and exact page inventory':
                    bool(frames)and frames[-1].startswith(f"{sym['_execute_parent_stop']:04x} ")and state.get(sym['_test_done'])==255 and word(sym['_test_failed'])==0,
                'only validated records reach remaining-owner release':bool(frames)and frames[0].startswith(f'{release if plan_valid else sym["_execute_parent_stop"]:04x} '),
                'user code runs only after successful cleanup, with command tail preserved':
                    (directory/'hostfs/RESULT.TXT').read_bytes()==(b''if expected else b'EXEC-COLD-OK\r\n'if cold else b'EXEC-HANDOFF-OK\r\n'),
                'all inputs unchanged':all(n.sha(Path(p))==value for p,value in hashes.items()),
                'all source media unchanged':all((directory/'hostfs'/name).read_bytes()==value for name,value in fixtures.items())}
        if plan_valid:
            before=n.memory(frames[0]);checks['handoff is bound to declared replacement identity']=bytes(before.get(0xfc0a+i,0)for i in range(32))==target[36:68]
        cases[label]={'passed':all(checks.values()),'checks':checks,'native_assertions':word(sym['_test_passed']),'failed_assertions':word(sym['_test_failed'])}
    report={'passed':all(r['passed']for r in cases.values()),'scope':__doc__,'cases':cases,
            'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes}
    (n.RUN/'execute-handoff-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']

if __name__=='__main__':main()
