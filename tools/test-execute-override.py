#!/usr/bin/env python3
"""Real OS override failure with a retained alias stack and caller rollback.

The fixture validates a HostFS image, then uses a missing/corrupt disposable
media twin to model a target change after checking. No OS/emulator patching.
Production Pascal Execute, successful handoff and file cleanup are not claimed.
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

def corrupt_tf(n,image):
    # Independently authored ZealFS v2/FAT8. File bytes remain intact, but the
    # chain ends after32 KiB, at the seek for the OS's third16 KiB read.
    disk=bytearray(n.blank_eeprom())
    count=(len(image)+255)//256
    struct.pack_into('<H',disk,4,254-count)
    entry=64;disk[entry]=0x80;disk[entry+1:entry+8]=b'BAD.COM'
    struct.pack_into('<HI',disk,entry+17,2,len(image))
    for i in range(count):
        page=i+2;disk[7+page//8]|=1<<(page%8)
        disk[256+page]=page+1 if i+1<count else 0
        disk[page*256:page*256+len(image[i*256:(i+1)*256])]=image[i*256:(i+1)*256]
    assert n.zealfs_files(disk)['/BAD.COM']==image
    disk[256+2+128-1]=0
    try:n.zealfs_files(disk)
    except AssertionError:pass # independent oracle rejects the shortened chain
    else:raise AssertionError('corrupt chain accepted')
    # TF's documented driver requires an MBR with a type5A partition.
    card=bytearray(1024*1024);card[510:512]=b'\x55\xaa';card[450]=0x5a
    struct.pack_into('<II',card,454,1,len(card)//512-1)
    card[512:512+len(disk)]=disk
    return bytes(card)

def main():
    n=module('native',ROOT/'tools/test-native.py')
    f=module('loader',ROOT/'tools/test-chain-loader.py')
    target='zexecuteprobe';sym=n.symbols(target);binary=(ROOT/f'build/{target}.bin').read_bytes()
    assert len(binary)<0x3800
    for area in ('DATA','BSS','INITIALIZED'):
        assert sym['s__'+area]+sym['l__'+area]<=0x7800
    image=bytes(f.handoff_executable(46080));disk=corrupt_tf(n,image)
    inputs=[Path(__file__),ROOT/'tools/test-native.py',ROOT/'tools/test-chain-loader.py',ROOT/'CMakeLists.txt',n.EMULATOR,n.ROM]
    inputs+=list((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+list((ROOT/'tests/native').glob('execute-probe.*'))
    inputs+=[ROOT/'tests/native/file-start.asm',ROOT/'tests/native/machine.asm']
    inputs+=[ROOT/f'build/{target}.{ext}'for ext in ('bin','map')]
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    reports={}
    for label,path,disabled in [('missing','H:/MISSING.COM',False),('late','T:/BAD.COM',False),
                               ('missing-disabled','H:/MISSING.COM',True),('late-disabled','T:/BAD.COM',True)]:
        is_late=label.startswith('late')
        commands=['run 100000000']+n.type_text('H:/'+target+'.bin',ticks=2000000)
        commands+=['bp 0x4000','tap ENTER','run 1000000000','bc 0x4000',
                   f"bp 0x{sym['_execute_os_override']:x}",'s','run 30000000000','regs','mmu',
                   f"rb 0x{sym['_originals']:x} 3",f"bc 0x{sym['_execute_os_override']:x}",
                   f"bp 0x{sym['execute_os_returned']:x}",'s','run 30000000000','regs','mmu','rb 0x8000 128',
                   f"bc 0x{sym['execute_os_returned']:x}",f"bp 0x{sym['execute_stack_returned']:x}",'s','run 30000000000',
                   'regs','mmu','rb 0x8000 128',f"bc 0x{sym['execute_stack_returned']:x}",
                   f"bp 0x{sym['_probe_stop']:x}",'s','run 1000000000','regs','mmu','rb 0xf000 12','rb 0xf200 32']
        for name in ('_test_done','_test_passed','_test_failed'):
            commands += [f'rb 0x{sym[name]:x} 2']
        commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
        fixtures={target+'.bin':binary,'x-max.COM':image,'CASE.TXT':(('!'if disabled else'')+path).encode()}
        raw=n.execute('execute-override-'+label,commands,fixtures,tf_image=disk,timeout=600)
        frames=re.split(r'\[DBG\] PC=',raw)[1:];checks={'all four actual checkpoints reached':len(frames)==4}
        if len(frames)==4:
            states=[n.memory(frame)for frame in frames]
            read=lambda k,at,size:bytes(states[k].get(at+i,0)for i in range(size))
            word=lambda at:int.from_bytes(read(3,at,2),'little')
            mmu=lambda k:{int(v,16):int(p,16)//16384 for v,p in re.findall(r'\[DBG\] (4000|8000|c000) -> ([0-9a-fA-F]{6})',frames[k])}
            maps=[mmu(i)for i in range(4)];originals=read(0,sym['_originals'],3)
            checks.update({
                'override and return execute on the retained prefix alias and isolated stack':
                    all(m.get(0x4000)==m.get(0xc000)and m.get(0x4000)not in originals for m in maps[:3])
                    and all(0xf800<=int(re.search(r'SP=([0-9a-f]+)',frame)[1],16)<=0xfc00 for frame in frames[:3]),
                'OS failure restores syscall entry mappings':maps[0]==maps[1]==maps[2],
                'OS read really overwrites caller data before late failure':read(1,0x8000,128)==(image[16384:16512]if is_late else bytes([0xa5])*128),
                'rollback restores caller data before old stack is resumed':read(2,0x8000,128)==bytes([0xa5])*128,
                'caller resumes with original three mappings':all(maps[3].get(v)==p for v,p in zip((0x4000,0x8000,0xc000),originals)),
                'native caller verifies restored code/data and preserved frames, stack reserve and exact page inventory':
                    read(3,sym['_test_done'],1)==b'\xff' and int.from_bytes(read(3,sym['_test_failed'],2),'little')==0
                    and int.from_bytes(read(3,sym['_test_passed'],2),'little')>=60 and word(0xf004)==360,
                'expected OS error remains available after rollback':word(0xf000)==(0x99 if is_late else 1),
                'caller interrupt state is preserved':read(3,0xf200,1)==read(3,0xf207,1)==bytes([not disabled]),
            })
        checks['all fixture bytes and disposable TF media unchanged']=(n.RUN/('execute-override-'+label)/'tf.img').read_bytes()==disk and all((n.RUN/('execute-override-'+label)/'hostfs'/name).read_bytes()==value for name,value in fixtures.items())
        checks['all qualification inputs unchanged']=all(n.sha(Path(p))==value for p,value in hashes.items())
        reports[label]={'passed':all(checks.values()),'checks':checks}
    report={'passed':all(r['passed']for r in reports.values()),'scope':__doc__,'cases':reports,
            'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes,
            'disposable_tf_sha256':hashlib.sha256(disk).hexdigest()}
    (n.RUN/'execute-override-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']

if __name__=='__main__':main()
