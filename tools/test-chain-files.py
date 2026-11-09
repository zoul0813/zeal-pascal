#!/usr/bin/env python3
"""Inline Chain associations and explicit file closing; Pascal Chain gate remains separate."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def host():
    out=ROOT/'analysis/chain-files/host';out.mkdir(parents=True,exist_ok=True)
    sdk=ROOT.parent/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
    command=['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-g','-DZP_FILE_HOST','-D__SDCC_VERSION_MAJOR=4','-D__SDCC_VERSION_MINOR=2','-D__sdcccall(x)=','-Isrc','-I'+str(sdk)]
    command+=['-D'+name+'=zf_model_'+name for name in ('open','close','read','write','seek','ioctl','exit')]
    command+=['src/common/path.c','src/runtime/files/operations.c','tests/native/chain-files.c','tests/native/file-model.c','-o',str(out/'chain-files')]
    subprocess.run(command,cwd=ROOT,check=True);subprocess.run([str(out/'chain-files')],cwd=ROOT,check=True)
def native():
    spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
    n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    target='zchainfilestest';sym=n.symbols(target)
    paths=[Path(__file__),ROOT/'CMakeLists.txt',ROOT/'tools/test-native.py',n.EMULATOR,n.ROM]
    paths+=list((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+[ROOT/'tests/native/chain-files.c',ROOT/'tests/native/file-start.asm',ROOT/'tests/native/file-probe.asm',ROOT/'tests/native/machine.asm']
    paths+=[ROOT/f'build/{target}.{ext}'for ext in ('bin','map')]+[ROOT/'build/file_runtime.zpl',ROOT/'build/file_runtime.module.json']
    hashes={str(p):n.sha(p)for p in paths if p.is_file()}
    for area in ('HEADER','SYSTEM','TEXT','DATA','BSS'):
        at,size=sym['s__'+area],sym['l__'+area];assert not size or 0x4000<=at<=at+size<=0x8000
    assert sym['_test_stop']==0xfc00 and sym['s__TEST']+sym['l__TEST']<0xc000
    assert sym['s__TEST_DATA']+sym['l__TEST_DATA']<0xe400
    commands=['run 100000000']+n.type_text('H:/'+target+'.bin\n')+[f"bp 0x{sym['_test_stop']:x}",'run 30000000000','s','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed','_test_index'):commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
    raw=n.execute('chain-files',commands,{target+'.bin':(ROOT/f'build/{target}.bin').read_bytes()},timeout=300)
    memory=n.memory(raw);word=lambda name:memory.get(sym[name],0)|(memory.get(sym[name]+1,0)<<8)
    expected={'CGLOBAL.DAT':bytes.fromhex('020002000b001600'),'CHEAP.DAT':bytes.fromhex('0200020021002c00'),
        'CSTATIC.DAT':bytes.fromhex('010002003700'),'CLOCAL.DAT':bytes.fromhex('010002004200'),'CPRIV.DAT':bytes.fromhex('010002004d00')}
    folder=n.RUN/'chain-files/hostfs'
    checks={'native completion':memory.get(sym['_test_done'])==255,
        'private path, surviving global/heap files, explicitly closed local/static files, canonical standard rebinding and stack/MMU/IFF':word('_test_failed')==0 and word('_test_passed')>=80,
        'all five files have independently specified finalized bytes':all((folder/name).read_bytes()==value for name,value in expected.items()),
        'all qualification inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items())}
    report={'passed':all(checks.values()),'scope':__doc__,'checks':checks,'native_assertions':word('_test_passed'),'failed':word('_test_failed'),'input_sha256':hashes,'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'chain-files-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert report['passed']
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--native',action='store_true');args=parser.parse_args()
    native()if args.native else host()
