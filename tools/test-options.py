#!/usr/bin/env python3
"""Native Start/End relocation, checked stack startup and IDE option ownership."""
import argparse
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


n=module('native',ROOT/'tools/test-native.py')
maps=module('maps',ROOT/'tools/test-sources.py')


def component():
    target='zoptionstest';sym=n.symbols(target)
    commands=['run 100000000']+n.program_launch(target)+[
        f"bp 0x{sym['_test_stop']:x}",'run 150000000000','s','regs','mmu']
    for name in ('_test_done','_test_stage','_test_passed','_test_failed'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
    names=['OBASE','OLOW','OBFFF','OC000','OC001','OD100','OMIN','OODD','OMAX','OBAD']
    source=(ROOT/'tests/native/options.pas').read_bytes()
    fixtures={name+'.PAS':source for name in names}
    fixtures.update({f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')})
    raw=n.execute('compiler-options',commands,fixtures,timeout=1800)
    memory=n.memory(raw);word=lambda name:memory.get(sym[name],0)|memory.get(sym[name]+1,0)<<8
    disk=n.RUN/'compiler-options/hostfs'
    checks={
        'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved execution and saved-PC lookup retain caller stack, MMU, IFF, source and owned pages':word('_test_failed')==0 and word('_test_passed')>=160,
        'every valid layout executes the selected initial stack and recursive VAR/static code correctly':
            (disk/'OPTIONS.TXT').is_file()and(disk/'OPTIONS.TXT').read_bytes()==b'OPTIONS-OK\r\n'*18,
        'all fixture sources remain unchanged':all((disk/(name+'.PAS')).read_bytes()==source for name in names),
    }
    artifacts={}
    try:
        for name in names:artifacts[name]=maps.mapping(disk,name)
        expected=[(0,0xfbff),(0x9000,0xd000),(0xbfff,0xefff),(0xc000,0xefff),
                  (0xc001,0xefff),(0xd100,0xfbff),(0,0xcfff),(0,0xfbfe)]
        for name,(start,end) in zip(names,expected):
            image=(disk/(name+'.COM')).read_bytes();w=lambda at:struct.unpack_from('<H',image,at)[0]
            origin,code,globals=w(20),w(22),w(26)
            assert origin==(start or 0x8000+globals)and w(30)==end
            assert w(28)==0x4000+len(image)==origin+code
            assert ((w(28)+1)&~1)+w(32)<=end+1 and w(32)==2048
            assert image[0x4000+globals:origin-0x4000]==bytes(origin-0x8000-globals)
        image=(disk/'OMAX.COM').read_bytes()
        assert struct.unpack_from('<H',image,28)[0]==0xf400
        checks['independent image/map/source identities, exact origins, zero gaps, End and stack extents authenticate']=True
        checks['maximum Start exactly fills the permitted static image extent']=True
    except(OSError,AssertionError,ValueError,struct.error)as error:
        checks['independent artifact validation']=False;artifacts['error']=repr(error)
    return checks,{'native_assertions_passed':word('_test_passed'),'native_assertions_failed':word('_test_failed'),'artifacts':artifacts,
        'expected_startup_failures':['End FFFF','minimum stack2047','End CFFE','insufficient aligned heap headroom','heap FFFF alignment overflow','minimum stack FFFF subtraction overflow']}


def ide():
    sym=n.symbols('zpascal');key=sym['_key_read'];work=sym['_work'];build=sym['_build'];opts=sym['_compile_options']
    labels=[];typed=lambda value:n.type_text(value,ticks=100000000)
    control=lambda letter:n.control(letter)[:-1]+['run 100000000']
    def checkpoint(label,trigger,cycles=4000000000):
        labels.append((label,key))
        return [f'bp 0x{key:x}']+trigger+[f'run {cycles}','regs','mmu',f'rb 0x{opts:x} 4',
            f'rb 0x{work:x} 140',f'rb 0x{build:x} {n.BUILD_BYTES}',f"rb 0x{sym['_main_file']:x} 128",
            f"rb 0x{sym['_ram_result']:x} 128",f'bc 0x{key:x}']
    source=(ROOT/'tests/native/options.pas').read_bytes();prefix=b'{unsaved}\n'
    commands=['run 100000000']+n.program_launch()+['run 10000000']
    commands+=typed('nwH:/project/main.pas\ne')+typed(prefix.decode())+control('K')+typed('d')
    commands+=checkpoint('memory-build',['tap C'])+['run 100000000']+typed('o')
    commands+=checkpoint('saved-selected',['tap C'],100000000)
    commands+=typed('s7fff')+checkpoint('low-start-rejected',['tap ENTER'])
    commands+=typed('c001')+checkpoint('high-start-selected',['tap ENTER'],100000000)
    commands+=typed('ecffe')+checkpoint('low-end-rejected',['tap ENTER'],100000000)
    commands+=typed('efff')+checkpoint('end-selected',['tap ENTER'],100000000)
    clear=n.kernel_symbols()['_video_vram_set'];labels.append(('options-screen',clear))
    commands += [f'bp 0x{clear:x}','tap C','run 100000000','regs','mmu']
    for offset in range(0,640,128):commands += [f'rb 0x{0x4000+offset:x} 128']
    commands += [f'bc 0x{clear:x}','run 100000000']
    commands+=typed('s10000')+checkpoint('wide-hex-rejected',['tap ENTER'],100000000)
    commands+=checkpoint('invalid-field-cancelled',['tap ESC'],100000000)
    commands+=typed('s')+checkpoint('empty-start',['tap ENTER'],100000000)
    commands+=typed('e')+checkpoint('escaped-end',['tap ESC'],100000000)
    commands+=typed('s')+checkpoint('interrupted-start',control('U')[:-1],100000000)
    commands+=typed('s0')+checkpoint('automatic-start',['tap ENTER'],100000000)
    commands+=typed('sc001')+checkpoint('restored-start',['tap ENTER'],100000000)
    commands+=typed('q')+checkpoint('saved-built',['tap C'])+['tap ESC','run 100000000']
    commands+=checkpoint('saved-run',['tap R'])+['tap ESC','run 100000000']
    commands+=typed('oeefff')+checkpoint('same-option-retains-cache',['tap ENTER'],100000000)
    commands+=typed('efffe')+checkpoint('ceiling-rejected',['tap ENTER'],100000000)
    commands+=typed('effe')+checkpoint('end-change-invalidates',['tap ENTER'],100000000)
    commands+=typed('q')+checkpoint('R-recompiled',['tap R'])
    commands+=checkpoint('R-returned',['tap ESC'])+['tap ESC','run 100000000']
    commands+=typed('omq')+checkpoint('memory-ignores-saved-options',['tap R'])
    commands+=['tap ESC','run 100000000']+typed('qn')+['quit']
    raw=n.execute('ide-options',commands,{'project/main.pas':source},timeout=1200)
    frames=re.split(r'\[DBG\] PC=',raw)[1:];checks={'all IDE option checkpoints reached':len(frames)==len(labels)}
    if len(frames)==len(labels):
        by_name={label:(n.memory(frame),frame)for(label,_),frame in zip(labels,frames)}
        data=lambda label,address,size:bytes(by_name[label][0].get(address+i,0)for i in range(size))
        word=lambda label,address:int.from_bytes(data(label,address,2),'little')
        settings=lambda label:(word(label,opts),word(label,opts+2))
        checks['all checkpoints stop at actual reader or video syscall']=all(frame.startswith(f'{pc:04x} ')for(_,pc),frame in zip(labels,frames))
        checks['Memory is initially automatic Start and default End']=settings('memory-build')==(0,0xfbff)
        checks['mode selection invalidates completed build']=data('memory-build',build+4,1)==b'\1'and data('saved-selected',build+4,1)==b'\0'
        checks['below-minimum Start and End retain previous settings and retry']=settings('low-start-rejected')==(0,0xfbff)and settings('low-end-rejected')==(0xc001,0xfbff)
        checks['valid bounded addresses are accepted']=settings('high-start-selected')==(0xc001,0xfbff)and settings('end-selected')==(0xc001,0xefff)
        code=word('end-selected',build+9);globals=word('end-selected',build+11)
        screen=data('options-screen',0x4000,640)
        checks['actual video shows selected values and calculated native minima/maxima']=(
            f'Start: C001 (min {0x8000+globals:04X} max {0xe800-code:04X})'.encode()in screen and
            f'End: EFFF (min {max(0xcfff,((0xc001+code+1)&~1)+2048-1):04X} max FBFF)'.encode()in screen and
            '4000 -> 100000'in by_name['options-screen'][1])
        checks['oversized hex is rejected without wrap']=settings('wide-hex-rejected')==(0xc001,0xefff)
        checks['empty, Escape and Ctrl-U retain settings']=all(settings(label)==(0xc001,0xefff)for label in ('invalid-field-cancelled','empty-start','escaped-end','interrupted-start'))
        checks['zero selects automatic Start']=settings('automatic-start')==(0,0xefff)and settings('restored-start')==(0xc001,0xefff)
        checks['End above native ceiling is rejected']=settings('ceiling-rejected')==(0xc001,0xefff)
        checks['changed End invalidates cache and R recompiles before running']=settings('end-change-invalidates')==(0xc001,0xeffe)and data('end-change-invalidates',build+4,1)==b'\0'and data('R-recompiled',build+4,1)==b'\1'
        cache=lambda label:data(label,build+4,79)+data(label,build+99,16)
        checks['unchanged saved build runs without recompilation']=cache('saved-run')==cache('saved-built')
        checks['unchanged option retains completed cache']=data('same-option-retains-cache',build,n.BUILD_BYTES)==data('saved-run',build,n.BUILD_BYTES)
        checks['saved settings never relocate Memory mode']=word('memory-ignores-saved-options',build+7)==0x8000+word('memory-ignores-saved-options',build+11)and data('memory-ignores-saved-options',sym['_ram_result']+26,1)==b'\1'
        checks['all options preserve dirty source identity and cursor']=all(data(label,work,140)==data('memory-build',work,140)for label,pc in labels if pc==key)
        checks['recompiled source retains exact unsaved snapshot']=data('R-recompiled',build+51,32)==hashlib.sha256(prefix+source).digest()
    disk=n.RUN/'ide-options/hostfs'
    checks['options and compilation never silently save source']=(disk/'project/main.pas').read_bytes()==source
    try:
        image=(disk/'project/main.COM').read_bytes()
        checks['saved publication contains selected Start and final End']=struct.unpack_from('<H',image,20)[0]==0xc001 and struct.unpack_from('<H',image,30)[0]==0xeffe
    except OSError:checks['saved publication contains selected Start and final End']=False
    return checks,{'checkpoints':labels}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ide',action='store_true');args=parser.parse_args()
    target='zpascal'if args.ide else'zoptionstest'
    paths=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py',ROOT/'tools/test-sources.py',ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    paths+=list((ROOT/'src').glob('*'))+list((ROOT/'tests/native').rglob('*'))
    paths+=[ROOT/f'build/{target}.{ext}'for ext in ('bin','zpl','map','banks.json')]
    paths += [ROOT/'build/pascal_runtime.zpl',ROOT/'build/pascal_runtime.private']
    hashes={str(p):n.sha(p)for p in paths if p.is_file()}
    checks,details=ide()if args.ide else component()
    checks['all qualification inputs unchanged']=all(n.sha(Path(p))==digest for p,digest in hashes.items())
    report={'passed':all(checks.values()),'scope':__doc__+' Bounded emulator acceptance; CHN/overlays and full conformance remain open.',
        'checks':checks,'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes,**details}
    (n.RUN/('ide-options-report.json'if args.ide else'options-report.json')).write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':main()
