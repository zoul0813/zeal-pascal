#!/usr/bin/env python3
"""Retained Chain coordinator faults and real nonreturning native handoff.

The native bootstrap/asset acquisition is test-only. Production Pascal Chain
syntax, asset transport and acquisition remain separate compiler integration.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[2]
sha=lambda data:hashlib.sha256(data).digest()
u16=lambda data,at:struct.unpack_from('<H',data,at)[0]


def host():
    out=ROOT/'analysis/transfer/host';out.mkdir(parents=True,exist_ok=True)
    sdk=ROOT.parent/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
    cmd=['cc','-std=c99','-Wall','-Wextra','-Werror','-fno-builtin-exit',
         '-fsanitize=address,undefined','-fno-sanitize-recover=all','-g','-DZP_TRANSFER_HOST',
         '-D__SDCC_VERSION_MAJOR=4','-D__SDCC_VERSION_MINOR=2','-D__sdcccall(x)=',
         '-Isrc','-I'+str(sdk),'src/runtime/process/transfer.c','tests/native/transfer-host.c','-o',str(out/'model')]
    for defines in (['-DZP_CHAIN_ONLY'],[]):
        subprocess.run(cmd+defines,cwd=ROOT,check=True);subprocess.run([str(out/'model')],cwd=ROOT,check=True)


def native_case(n,label,with_file):
    inputs=[Path(__file__),ROOT/'CMakeLists.txt',ROOT/'tests/acceptance/test-native.py',ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=list((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+list((ROOT/'tests/native').glob('transfer-*'))
    inputs+=[ROOT/f'build/{target}.{ext}'for target in ('zchaintest','ztransferparent')for ext in ('bin','map')]
    inputs+=[ROOT/'build'/name for name in ('zchaintest.zpl','transfer_runtime.zpl','transfer_runtime.module.json',
             'transfer_gate.zpl','transfer_gate.map','ztransferdriver.zpl','ztransferdriver.map','pascal_runtime.profile')]
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    target='zchaintest';sym=n.symbols(target)
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{sym['_test_stop']:x}",'run 100000000000','s','regs','mmu']
    for name in ('_test_done','_test_stage','_test_passed','_test_failed'):commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
    source=(ROOT/'tests/native'/('transfer-entry.pas'if with_file else'transfer-bare.pas')).read_bytes()
    files={target+'.bin':(ROOT/f'build/{target}.bin').read_bytes(),target+'.zpl':(ROOT/f'build/{target}.zpl').read_bytes(),
           'BASE.PAS':source,'CHAIN.PAS':source}
    raw=n.execute(label+'-compile',commands,files,timeout=900);memory=n.memory(raw)
    word=lambda name:memory.get(sym[name],0)|(memory.get(sym[name]+1,0)<<8)
    assert memory.get(sym['_test_done'])==255 and word('_test_failed')==0,raw[-2000:]
    folder=n.RUN/(label+'-compile')/'hostfs'
    base=bytearray((folder/'BASE.COM').read_bytes());chain=(folder/'CHAIN.CHN').read_bytes()
    assert chain[192:196]==(ROOT/'build/pascal_runtime.profile').read_bytes()
    driver=(ROOT/'build/ztransferdriver.zpl').read_bytes()
    assert 0<len(driver)<=0x800 and u16(base,20)==0x9000 and u16(base,26)<0x800
    base[0x4800:0x4800+len(driver)]=driver
    base[36:132]=bytes(96)
    # The caller is deliberately modified and independently rehashed. Its map
    # is not used to pretend this bootstrap came from Pascal source.
    parent='ztransferparent';ps=n.symbols(parent);binary=(ROOT/f'build/{parent}.bin').read_bytes()
    assert len(binary)<=16384 and ps['_transfer_parent_stop']==0x7ff0
    commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
    terminal=n.symbols('transfer_gate')['transfer_gate_returned']
    commands+=['bp 0x4000','tap ENTER','run 1000000000','bc 0x4000',f"bp 0x{ps['_transfer_parent_ready']:x}",'s','run 10000000000','regs','mmu',
               f"rb 0x{ps['_module_page']:x} 1",f"bc 0x{ps['_transfer_parent_ready']:x}",'bp 0x7ff0','bp 0x9000','s','run 10000000000','regs','mmu','rb 0xfe70 32',
               'bp 0x8800','s','run 10000000000','regs','mmu','rb 0x8000 128','rb 0xfe70 32','bc 0x8800',
               's','run 30000000000','regs','mmu','rb 0x4000 160','rb 0x8000 128','rb 0xfe70 32','rb 0xff00 7',
               'bc 0x9000',f'bp 0x{terminal:x}','s','run 30000000000','regs','mmu','rb 0xfe70 32',f'bc 0x{terminal:x}',
               's','run 30000000000','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{ps[name]:x} 2']
    commands += [f"rb 0x{ps['_test_failures']:x} 128",'quit']
    shared_file=b'\x03\0\x01\0ABC' # typed file: record count, component width, data
    files={parent+'.bin':binary,'CALLER.COM':bytes(base),'CHAIN.CHN':chain,'SHARED.DAT':shared_file,
           'transfer_runtime.zpl':(ROOT/'build/transfer_runtime.zpl').read_bytes()}
    raw=n.execute(label+'-handoff',commands,files,timeout=900)
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    checks={'all actual native checkpoints reached':len(frames)==6}
    if len(frames)==6:
        states=[n.memory(frame)for frame in frames]
        read=lambda k,at,size:bytes(states[k].get(at+i,0)for i in range(size))
        page=read(0,ps['_module_page'],1)[0]
        checks.update({
            'caller, prepared file and replacement use the real execution locations':frames[1].startswith('9000 ')and frames[2].startswith('8800 ')and frames[3].startswith('9000 ')and frames[4].startswith(f'{terminal:04x} ')and frames[5].startswith('7ff0 '),
            'nonreturning commit installs a fresh stack/frame root':'SP=fc00'in frames[3].split('\n',1)[0]and bool(re.search(r'\bIX=0000\b',frames[3])),
            'shared globals and any complete file object survive activation':read(2,0x8000,39 if with_file else 23)==read(3,0x8000,39 if with_file else 23)and read(3,0x8000,9)==b'\x08retained',
            'child owns its coordinator and preserves the real file-module state':read(3,0xfe75,1)!=bytes([page])and read(3,0xfe75,1)!=b'\0'and read(3,0xfe74,1)==read(2,0xfe74,1)and (read(2,0xfe74,1)!=b'\0')==with_file,
            'command tail is preserved byte-for-byte':read(3,0xff00,7)==bytes([6,page])+b'CHAIN',
            'prefix geometry describes replacement code/globals':u16(read(3,0x4000,160),22)==u16(chain,12)and u16(read(3,0x4000,160),26)==u16(chain,16),
            'project terminal cleanup releases both module handles before OS exit':read(4,0xfe74,2)==b'\0\0'and read(4,0xfe77,1)==b'\0'and read(4,0xfe7f,1)==b'\0',
            'parent observes clean success and exact physical-page recovery':read(5,ps['_test_done'],1)==b'\xff'and int.from_bytes(read(5,ps['_test_failed'],2),'little')==0,
        })
    output=n.RUN/(label+'-handoff')/'hostfs/TRANSFER.TXT'
    expected=b'CHAIN-TRANSFER-OK:retained:1234:B:18\r\n'if with_file else b''
    checks['replacement result matches and parent-after never runs']=output.is_file()and output.read_bytes()==expected
    checks['shared disk input remains unchanged']=(output.parent/'SHARED.DAT').read_bytes()==shared_file
    checks['all source/build/platform inputs unchanged']=all(n.sha(Path(p))==digest for p,digest in hashes.items())
    report={'passed':all(checks.values()),'scope':__doc__,'checks':checks,'compiler_assertions':word('_test_passed'),
            'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes}
    (n.RUN/(label+'-report.json')).write_text(json.dumps(report,indent=2)+'\n')
    return report


def native():
    spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
    n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    cases={label:native_case(n,label,files)for label,files in [('shared',True),('bare',False)]}
    same=cases['shared']['input_sha256']==cases['bare']['input_sha256']
    report={'passed':same and all(r['passed']for r in cases.values()),'scope':__doc__,
            'same_inputs_across_cases':same,'capture_directory':str(n.RUN.relative_to(ROOT)),
            'cases':{label:{k:v for k,v in r.items()if k!='input_sha256'}for label,r in cases.items()},
            'input_sha256':cases['shared']['input_sha256']}
    (n.RUN/'transfer-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--native',action='store_true');args=parser.parse_args()
    native()if args.native else host()
