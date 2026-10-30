#!/usr/bin/env python3
"""Independent artifact fixtures and sanitized execution of the saved-PC reader."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
import importlib.util
import re

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'analysis/saved-pc/host'
sha=lambda b:hashlib.sha256(b).digest()
put16=lambda b,at,n:struct.pack_into('<H',b,at,n)
put32=lambda b,at,n:struct.pack_into('<I',b,at,n)
SOURCE=b'program Saved;\nbegin\n{$I parts/fail.inc}\nend.\n'
PART=b'writeln(1 div 0);\n'
NATIVE_ROOT=b'program Saved;\nvar n:integer;\nbegin\n{$I parts/fail.inc}\nend.\n'
NATIVE_PART=b'n:=1 div 0;\n'

def fixture(extra=0,reverse=False):
    records=[]
    inputs=[(b'H:/main.pas',SOURCE),(b'H:/parts/fail.inc',PART)]+[(f'H:/parts/e{i:03}.inc'.encode(),b'')for i in range(extra)]
    for i,(name,data) in enumerate(inputs):
        records.append(struct.pack('<HHI',i,len(name),len(data))+sha(data)+name)
    if reverse:records.reverse()
    table=b''.join(records)
    entries=b''.join(struct.pack('<4H2I',*row) for row in [(0,4,0,0,0,7),(4,8,1,0,8,15),(8,16,0,0,20,24)])
    m=bytearray(96)+table+entries;m[:4]=b'ZMAP';m[4]=1
    for at,value in [(6,96),(8,1),(10,0x8000),(12,32),(14,len(records))]:put16(m,at,value)
    for at,value in [(16,3),(84,len(table)),(88,len(entries))]:put32(m,at,value)
    m[52:84]=sha(table)
    b=bytearray(16384+32);b[0]=0xc3;b[3:7]=b'ZP80';b[7]=1
    for at,value in [(1,0x40a0),(8,160),(10,len(b)),(12,0x40a0),(14,0x40a3),(16,1),(20,0x8000),(22,32),(24,0x8000),(28,0x8020),(30,0xfbff),(32,2048),(132,16384)]:put16(b,at,value)
    b[68:100]=m[52:84];b[16384:]=b'\xc9'*32
    return b,m

def seal(b,m):
    m[20:52]=bytes(32);b[100:132]=sha(m);b[36:68]=bytes(32);b[36:68]=sha(b);m[20:52]=b[36:68]

def write(name,b,m,source=SOURCE,part=PART,chain=False):
    p=OUT/name;(p/'parts').mkdir(parents=True,exist_ok=True)
    (p/('main.CHN'if chain else'main.COM')).write_bytes(b);(p/'main.ZMP').write_bytes(m)
    (p/'main.pas').write_bytes(source);(p/'parts/fail.inc').write_bytes(part)
    return p

def native():
    spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
    n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    target='zmaptest';sym=n.symbols(target);video=n.kernel_symbols()['video_write']
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py',ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=list((ROOT/'src').glob('*'))+list((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}'for ext in ('bin','zpl','map','banks.json')]
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{sym['_test_stop']:x}",f'bp 0x{video:x}']
    for _ in range(2):commands+=['run 60000000000','regs','mmu','rb 0xbc80 32']
    commands += [f'bc 0x{video:x}','run 60000000000','s','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed','_test_stage','_test_pc','_test_io_pc'):
        commands+=[f'rb 0x{sym[name]:x} 2']
    commands+=[f"rb 0x{sym['_test_failures']:x} 128",'quit']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures.update({'project/main.pas':NATIVE_ROOT,'project/parts/fail.inc':NATIVE_PART,
        'ioerr.pas':b"program IOFailure;var f:Text;begin assign(f,'H:/absent.dat');reset(f)end.\n"})
    raw=n.execute('saved-pc',commands,fixtures,timeout=600)
    frames=[f for f in re.split(r'\[DBG\] PC=',raw)[1:]if f.startswith(f'{video:04x} ')]
    printed=[]
    for frame in frames:
        m=n.memory(frame);size=int(re.search(r'\bBC=([0-9a-fA-F]{4})',frame)[1],16)
        address=int(re.search(r'\bDE=([0-9a-fA-F]{4})',frame)[1],16)
        printed.append(bytes(m.get(address+i,0)for i in range(size)))
    memory=n.memory(raw);word=lambda name:memory.get(sym[name],0)|(memory.get(sym[name]+1,0)<<8)
    pc,io_pc=word('_test_pc'),word('_test_io_pc');disk=n.RUN/'saved-pc/hostfs'
    # Decode the emitted source map independently of either native lookup API.
    mapping=(disk/'project/main.ZMP').read_bytes();image=(disk/'project/main.COM').read_bytes()
    table=struct.unpack_from('<I',mapping,84)[0]
    authenticated=(mapping[:10]==b'ZMAP\x01\0\x60\0\x01\0' and
        struct.unpack_from('<H',mapping,14)[0]==2 and
        sha(image[:36]+bytes(32)+image[68:])==image[36:68]==mapping[20:52] and
        sha(mapping[:20]+bytes(32)+mapping[52:])==image[100:132] and
        sha(mapping[96:96+table])==image[68:100]==mapping[52:84])
    records={};at=96
    while at<96+table:
        file,size,length=struct.unpack_from('<HHI',mapping,at);name=mapping[at+40:at+40+size].decode('ascii')
        source=(disk/name[3:]).read_bytes().split(b'\x1a',1)[0].replace(b'\r\n',b'\n').replace(b'\r',b'\n')
        source=b'\n'.join(line.rstrip(b' ')for line in source.split(b'\n'))
        authenticated=authenticated and file not in records and len(source)==length and sha(source)==mapping[at+8:at+40]
        records[file]=name;at+=40+size
    authenticated=authenticated and at==96+table and records=={0:'H:/project/main.pas',1:'H:/project/parts/fail.inc'}
    matching=[struct.unpack_from('<4H2I',mapping,at)for at in range(96+table,len(mapping),16)
              if struct.unpack_from('<HH',mapping,at)[0]<=pc<struct.unpack_from('<HH',mapping,at)[1]]
    checks={
        'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'fresh lookup, stale sources, unsaved overrides, memory exhaustion and restored ownership':word('_test_failed')==0 and word('_test_passed')>=60,
        'saved native failures print their actual runtime and I/O PCs through VID0':printed==[
            f'Runtime error 02, PC={pc:04X}\r\n'.encode(),f'I/O error 01, PC={io_pc:04X}\r\n'.encode()],
        'reported runtime PC independently selects exact included assignment':len(matching)==1 and matching[0][2]==1 and matching[0][4]==0,
        'native saved image, map and every serialized source identity authenticate independently':authenticated,
        'disk sources survive lookup and all unsaved edits':(disk/'project/main.pas').read_bytes()==NATIVE_ROOT and(disk/'project/parts/fail.inc').read_bytes()==NATIVE_PART,
        'all qualification inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    report={'passed':all(checks.values()),'scope':'Native saved-PC lookup and actual standalone error reporting; physical IDE acceptance is separate.',
        'checks':checks,'printed_diagnostics':[x.decode('ascii',errors='replace')for x in printed],
        'native_assertions_passed':word('_test_passed'),'native_assertions_failed':word('_test_failed'),
        'pc':pc,'io_pc':io_pc,'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes}
    (n.RUN/'saved-pc-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert report['passed']
    return disk,pc

def ide(disk,pc):
    spec=importlib.util.spec_from_file_location('native_ide',ROOT/'tools/test-native.py')
    n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    sym=n.symbols('zpascal');key=sym['_key_read'];work=sym['_work'];build=sym['_build'];labels=[]
    fixtures={str(p.relative_to(disk)):p.read_bytes()for p in disk.rglob('*')if p.is_file()and p.suffix.upper()in('.PAS','.INC','.COM','.ZMP')}
    hashes={str(p):n.sha(p)for p in [ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py',n.EMULATOR,n.ROM,
        *list((ROOT/'src').glob('*')),*[ROOT/f'build/zpascal.{ext}'for ext in ('bin','zpl','map','banks.json')]]if p.is_file()}
    typed=lambda text:n.type_text(text,ticks=100000000)
    control=lambda letter:n.control(letter)[:-1]+['run 100000000']
    leave=lambda:control('K')+typed('d')+['run 1000000000']
    def checkpoint(label,trigger,cycles=4000000000):
        labels.append(label)
        return [f'bp 0x{key:x}',f'tap {trigger}',f'run {cycles}','regs','mmu',
            f"rb 0x{sym['_ide_state']:x} 1",f'rb 0x{work:x} 140',f'rb 0x{build:x} {n.BUILD_BYTES}',
            f"rb 0x{sym['_main_file']:x} 128",f"rb 0x{sym['_buffers']:x} {n.SOURCES_BYTES}",f"rb 0x{sym['_ram_result']:x} 128",f'bc 0x{key:x}']
    commands=['run 100000000']+n.program_launch()+['run 10000000']+typed('nwH:/project/main.pas\nof'+f'{pc:04X}')
    commands+=checkpoint('fresh-found','ENTER')+checkpoint('fresh-source','ESC',100000000)
    commands+=leave()+typed('wH:/project/main.pas\ne')+typed('{dirty}\n')+leave()+typed('of'+f'{pc:04X}')
    commands+=checkpoint('stale-dirty-root','ENTER')+['tap ESC','run 100000000']+typed('qe')
    commands+=control('Y')+leave()+typed('of'+f'{pc:04X}')
    commands+=checkpoint('matching-dirty-root','ENTER')+checkpoint('matching-source','ESC',100000000)
    commands+=control('Y')+typed('n:=1 div 1;\n')+leave()+checkpoint('unsaved-repair-run','R')
    commands+=['tap ESC','run 100000000']+typed('wnH:/project/main.pas\n')+checkpoint('restored-dirty-root','SPACE',100000000)
    commands+=typed('q')+checkpoint('quit-cancel','ESC',100000000)+['quit']
    raw=n.execute('ide-saved-pc',commands,fixtures,timeout=900)
    frames=[n.memory(f)for f in re.split(r'\[DBG\] PC=',raw)[1:]]
    checks={'all saved-PC IDE checkpoints reached':len(frames)==len(labels)}
    if len(frames)==len(labels):
        f=dict(zip(labels,frames));text=lambda m,a:bytes(m.get(a+i,0)for i in range(128)).split(b'\0')[0]
        word=lambda m,a:m.get(a,0)|(m.get(a+1,0)<<8)
        checks.update({
            'fresh Memory-mode F uses saved artifacts without recompiling':f['fresh-found'].get(sym['_ide_state'])==3 and f['fresh-found'].get(build+4)==0,
            'F selects actual included source and keeps Main':text(f['fresh-found'],work+12)==b'H:/project/parts/fail.inc' and text(f['fresh-found'],sym['_main_file'])==b'H:/project/main.pas',
            'Escape enters the exact included statement':f['fresh-source'].get(sym['_ide_state'])==2 and word(f['fresh-source'],work+4)==0,
            'changed unsaved root rejects lookup without losing edits':text(f['stale-dirty-root'],work+12)==b'H:/project/main.pas' and f['stale-dirty-root'].get(work+1)==1 and word(f['stale-dirty-root'],work+2)==len(NATIVE_ROOT)+8,
            'matching unsaved root permits lookup and remains parked':f['matching-dirty-root'].get(sym['_ide_state'])==3 and f['matching-source'].get(sym['_ide_state'])==2,
            'unsaved include repair compiles Main and runs successfully':f['unsaved-repair-run'].get(sym['_ram_result']+26)==1 and f['unsaved-repair-run'].get(work+1)==1,
            'parked root and quit cancellation preserve dirty buffers':text(f['restored-dirty-root'],work+12)==b'H:/project/main.pas' and f['restored-dirty-root'].get(work+1)==1 and f['quit-cancel'].get(sym['_ide_state'])==1 and f['quit-cancel'].get(work+1)==1,
        })
    folder=n.RUN/'ide-saved-pc/hostfs'
    checks['lookup and Memory repair never publish source or outputs']=all((folder/path).read_bytes()==data for path,data in fixtures.items())
    checks['all qualification inputs unchanged']=all(n.sha(Path(p))==h for p,h in hashes.items())
    report={'passed':all(checks.values()),'scope':'Physical native fresh-session F and unsaved included-source repair; remaining IDE/conformance gates stay open.',
        'checks':checks,'checkpoints':labels,'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes}
    (n.RUN/'ide-saved-pc-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert report['passed']

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native',action='store_true',help='Run native lookup/error-reporting followed by physical IDE F acceptance')
    parser.add_argument('--ide',type=Path,help='Rerun physical F acceptance from a completed native capture directory')
    args=parser.parse_args()
    if args.native:
        disk,pc=native();ide(disk,pc);return
    if args.ide:
        report=json.loads((args.ide/'saved-pc-report.json').read_text());ide(args.ide/'saved-pc/hostfs',report['pc']);return
    OUT.mkdir(parents=True,exist_ok=True)
    sdk=ROOT.parent/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
    cmd=['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-g','-D__SDCC_VERSION_MAJOR=4','-D__SDCC_VERSION_MINOR=2','-D__sdcccall(x)=','-Isrc','-I'+str(sdk)]
    cmd+=['-D'+name+'=map_model_'+name for name in ('open','read','close','seek')]
    cmd+=['src/compiler_job_wire.c','src/source.c','src/sources.c','src/sources_storage.c','src/path.c','src/sha256.c','src/source_hash.c','src/saved_map.c','src/image.c','src/native_image.c','src/compiler_image.c','tests/native/saved-host.c','-o',str(OUT/'saved-host')]
    subprocess.run(cmd,cwd=ROOT,check=True)
    b,m=fixture();seal(b,m);positive=write('valid',b,m)
    results=[]
    def run(path,mode):
        p=subprocess.run([str(OUT/'saved-host'),str(path),str(int(mode))],cwd=ROOT,capture_output=True,text=True)
        if p.returncode:print(p.stdout,p.stderr,flush=True);p.check_returncode()
        print(path.name,p.stdout.strip());results.append({'case':path.name,'result':p.stdout.strip()})
    run(positive,False)
    # An odd static end consumes one byte before the aligned heap starts.
    # Authenticate coherent large-image/map fixtures around that exact bound.
    for name,end,fails in [('aligned-stack-short',0xd000,True),('aligned-stack-exact',0xd001,False)]:
        x,y=fixture();x+=bytes(0x8801-len(x));x[0x4000:0x4020]=bytes(32)
        x[0x87e1:0x8801]=b'\xc9'*32
        for at,value in [(10,len(x)),(20,0xc7e1),(28,0xc801),(30,end)]:put16(x,at,value)
        put16(y,10,0xc7e1);seal(x,y);run(write(name,x,y),fails)
    for name,part in [('stale-include',PART+b'{changed}\n')]:run(write(name,b,m,part=part),True)
    for target,fields in [('image',[(0,0),(7,2),(34,1),(134,1),(140,1),(18,2)]),('map',[(0,0),(4,2),(5,1),(92,1)])]:
        for at,value in fields:
            x,y=bytearray(b),bytearray(m);(x if target=='image' else y)[at]=value;seal(x,y)
            run(write(f'{target}-{at}',x,y),True)
    for name,offset,value in [('overlap',96+len(m[96:-48])+16,3),('unknown-file',len(m)-48+4,2),('outside-code',len(m)-48+2,33),('reserved-entry',len(m)-48+6,1)]:
        x,y=bytearray(b),bytearray(m);put16(y,offset,value);seal(x,y);run(write(name,x,y),True)
    for name,at,value in [('source-bounds',len(m)-48+12,999),('table-overflow',84,0xffffffff),('entry-overflow',16,0xffffffff)]:
        x,y=bytearray(b),bytearray(m);put32(y,at,value);seal(x,y);run(write(name,x,y),True)
    for name,which in [('image-trailing',0),('map-trailing',1),('image-truncated',2),('map-truncated',3)]:
        x,y=bytearray(b),bytearray(m)
        if which==0:x+=b'x'
        elif which==1:y+=b'x'
        elif which==2:x=x[:-1]
        else:y=y[:-1]
        run(write(name,x,y),True)
    # A matching selected source does not excuse a stale include or missing map.
    p=write('missing-map',b,m);(p/'main.ZMP').unlink();run(p,True)
    for name,extra,reverse in [('reordered',0,True),('wide-metadata',300,False)]:
        x,y=fixture(extra,reverse);seal(x,y);p=write(name,x,y)
        for i in range(extra):(p/'parts'/f'e{i:03}.inc').write_bytes(b'')
        run(p,2)
    # Independently authored code-only wire fixtures. Only complete-stream
    # hashing, shape and source-map lookup are tested here, not execution.
    def chain_fixture():
        _,mapping=fixture();image=bytearray(96)+b'\xc9'*32;image[:4]=b'ZCHN';image[4]=1
        for at,value in [(6,96),(8,1),(10,0x8000),(12,32),(14,0x8000),(16,0),(18,0xfbff)]:put16(image,at,value)
        return image,mapping
    def seal_chain(image,mapping):
        mapping[20:52]=bytes(32);image[52:84]=sha(mapping);image[20:52]=bytes(32);image[20:52]=sha(image);mapping[20:52]=image[20:52]
    x,y=chain_fixture();seal_chain(x,y);run(write('chain-valid',x,y,chain=True),4)
    for at,value in [(4,2),(5,1),(6,95),(8,2),(10,0x7fff),(12,0),(12,0xffff),
                     (14,0x8100),(16,0xffff),(18,0xcffe),(18,0xffff),(84,1),(90,1),(92,1)]:
        x,y=chain_fixture();put16(x,at,value);seal_chain(x,y)
        run(write(f'chain-field-{at}-{value}',x,y,chain=True),5)
    for label,delta in [('truncated',-1),('trailing',1)]:
        x,y=chain_fixture();seal_chain(x,y);x=x[:-1]if delta<0 else x+b'x'
        run(write('chain-'+label,x,y,chain=True),5)
    x,y=chain_fixture();seal_chain(x,y);x[-1]^=1;run(write('chain-unsealed-payload',x,y,chain=True),5)
    x,y=chain_fixture();y[52]^=1;seal_chain(x,y);run(write('chain-bad-manifest',x,y,chain=True),5)
    x,y=chain_fixture();x+=bytes(0x7400-32);put16(x,12,0x7400);put16(y,12,0x7400);seal_chain(x,y)
    run(write('chain-maximum-static',x,y,chain=True),6)
    report={'passed':True,'scope':__doc__+' Structural/failure tests; native IDE acceptance is separate.','cases':results}
    (OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
