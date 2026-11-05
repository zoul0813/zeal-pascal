#!/usr/bin/env python3
"""Physical native X Execute, ordinary child return and unchanged IDE ownership."""
from pathlib import Path
import importlib.util
import hashlib
import json
import re

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)


def main():
    sym=n.symbols('zpascal');kernel=n.kernel_symbols();key=sym['_key_read']
    n.SHIFTED['?']='SLASH'
    work=sym['_work'];build=sym['_build'];editor=sym['_editor'];labels=[]
    source=b"program Keep;\nvar n:integer;\nbegin n:=7;writeln('IDE-CACHE-OK')end.\n"
    prefix=b'{unsaved}\n'
    child=(ROOT/'build/zpexecchild.bin').read_bytes()
    assert child[3:7]!=b'ZP80' and 0<len(child)<49152
    fixtures={'project/main.pas':source,'external':child,'empty.bin':b''}
    paths=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py',n.EMULATOR,n.ROM]
    paths+=list((ROOT/'src').glob('*'))+list((ROOT/'tests/native').glob('*'))
    paths += [ROOT/f'build/zpascal.{ext}'for ext in ('bin','zpl','map','banks.json')]+[ROOT/'build/zpexecchild.bin']
    paths += [ROOT.parent/'zeal-dev-environment/home/Zeal-VideoBoard-SDK/include/zvb_hardware.h']
    hashes={str(p):n.sha(p)for p in paths if p.is_file()}
    typed=lambda text:n.type_text(text,ticks=100000000)
    control=lambda letter:n.control(letter)[:-1]+['run 100000000']
    def checkpoint(label,trigger,cycles=4000000000):
        labels.append((label,'state'))
        if isinstance(trigger,str):trigger=['tap '+trigger]
        return [f'bp 0x{key:x}']+trigger+[f'run {cycles}','regs','mmu',
            f"rb 0x{sym['_ide_state']:x} 1",f'rb 0x{work:x} 140',f'rb 0x{build:x} {n.BUILD_BYTES}',
            f'rb 0x{editor:x} 223',f"rb 0x{sym['_main_file']:x} 128",
            f"rb 0x{sym['_ram_result']:x} 128",f'bc 0x{key:x}']
    def screen(label,trigger,rows=3):
        labels.append((label,'screen'));clear=kernel['_video_vram_set']
        result=[f'bp 0x{clear:x}']+trigger
        if label != 'source-retained':
            # Escape dismisses status or is ignored at the menu. Open Options
            # to force a supported clear, sampling the completed prior screen.
            result += ['run 100000000', 'tap O']
        result += ['run 1000000000','regs','mmu']
        for base in (0x4000,0x5000):
            for at in range(0,rows*80,256):result+=[f'rb 0x{base+at:x} {min(256,rows*80-at)}']
        result += [f"rb 0x{kernel['_vfs_current_dir']:x} 128",f"rb 0x{kernel['_page_owners']:x} 32",f"rb 0x{kernel['mmu_bitmap']:x} 4",f'bc 0x{clear:x}','run 100000000']
        if label != 'source-retained':
            result += n.type_text('q', ticks=100000000) + ['run 100000000']
        return result
    commands=['run 100000000']+typed('cd H:/\n')+['run 100000000']+n.program_launch()+['run 10000000']
    commands+=typed('nwH:/project/main.pas\ne')+typed(prefix.decode())
    commands+=control('K')+typed('b')+control('K')+typed('d')
    commands+=typed('mH:/project/main.pas\n')+checkpoint('cached-build','C')
    commands+=screen('before-execute',['tap ESC'])
    commands+=typed('xexternal')+checkpoint('ordinary-child',['press SHIFT','tap ENTER','release SHIFT'])
    commands+=screen('ordinary-status',['tap ESC'])
    commands+=typed('xproject/../external')+checkpoint('repeat-child','ENTER')
    commands+=screen('repeat-status',['tap ESC'])
    for label,path in [('missing','H:/absent.bin'),('empty','H:/empty.bin'),('directory','H:/project'),('invalid','H:/1234567890123456')]:
        commands+=typed('x'+path)+checkpoint(label,'ENTER')+screen(label+'-status',['tap ESC'])
    commands+=typed('x')+checkpoint('escape-cancel','ESC',100000000)
    commands+=typed('x')+checkpoint('empty-cancel','ENTER',100000000)
    commands+=typed('x')+checkpoint('control-cancel',control('U')[:-1],100000000)
    commands+=checkpoint('cached-run','R')+['tap ESC','run 100000000']
    commands+=typed('e')+screen('source-retained',control('Q')[:-1],rows=8)
    commands+=typed('?')+control('K')+typed('d')
    labels.append(('ide-exit','exit'))
    commands+=[f"bp 0x{kernel['zos_loader_exit']:x}"]+typed('qn')+['regs','mmu',f"rb 0x{kernel['_page_owners']:x} 32",'quit']
    raw=n.execute('ide-execute',commands,fixtures,timeout=900)
    parts=re.split(r'\[DBG\] PC=',raw)[1:];checks={'all native X checkpoints reached':len(parts)==len(labels)}
    screenshots={}
    if len(parts)==len(labels):
        frames={label:(n.memory(part),part)for(label,_),part in zip(labels,parts)}
        checks['state snapshots stop at actual resident reader']=all(part.startswith(f'{key:04x} ')for(_,kind),part in zip(labels,parts)if kind=='state')
        data=lambda label,address,size:bytes(frames[label][0].get(address+i,0)for i in range(size))
        baseline={address:data('cached-build',address,size)for address,size in ((work,140),(build,n.BUILD_BYTES),(editor,223),(sym['_main_file'],128))}
        checkpoints=['ordinary-child','repeat-child','missing','empty','directory','invalid','escape-cancel','empty-cancel','control-cancel']
        checks['ordinary and failed/canceled execution retain exact Work/Main/editor and cached build']=all(
            data(label,address,len(expected))==expected for label in checkpoints for address,expected in baseline.items())
        checks['baseline is a valid unsaved build with a retained block mark']=baseline[work][1]==1 and baseline[editor][152]==1 and baseline[build][4]==1 and baseline[build][15:19]==baseline[work][8:12]
        for label,kind in labels:
            if kind!='screen':continue
            m,part=frames[label];screen_bytes=data(label,0x4000,640 if label=='source-retained' else 240)
            checks[label+' uses real character VRAM']='4000 -> 100000' in part
            screenshots[label]=screen_bytes.replace(b'\0',b' ').decode('ascii',errors='replace')
            if label!='source-retained':
                checks[label+' retains original native directory']=data(label,kernel['_vfs_current_dir'],128).split(b'\0')[0]==b'H:/'
        checks['native child allocations and loader pages are reclaimed']=all(data(label,kernel['_page_owners'],32)==data('before-execute',kernel['_page_owners'],32)and data(label,kernel['mmu_bitmap'],4)==data('before-execute',kernel['mmu_bitmap'],4)for label in ('ordinary-status','repeat-status','missing-status','empty-status','directory-status','invalid-status'))
        checks['ordinary SDK child returns status 37 without a Pascal header']=all('Program returned, status 37' in screenshots[label]for label in ('ordinary-status','repeat-status'))
        checks['next native child observes restored physical text mode after graphics']= 'Program returned, status 37' in screenshots['repeat-status']
        checks['loader and invalid-path failures are explicit']=all('Execute failed, native error ' in screenshots[label+'-status']for label in ('missing','empty','directory','invalid'))
        checks['child colors are restored before IDE output']=all(data(label,0x5000+80,26)==bytes([15])*26 for label in ('ordinary-status','repeat-status'))
        checks['Escape, empty name and Ctrl-U cancel at menu']=all(data(label,sym['_ide_state'],1)==b'\x01'for label in ('escape-cancel','empty-cancel','control-cancel'))
        checks['R reuses preserved valid build and returns normally']=n.same_build_after_run(baseline[build],data('cached-run',build,n.BUILD_BYTES)) and data('cached-run',sym['_ram_result']+26,1)==b'\x01'
        checks['IDE exits normally after explicit discard']=frames['ide-exit'][1].startswith(f"{kernel['zos_loader_exit']:04x} ")and re.search(r'HL=00',frames['ide-exit'][1])is not None
        checks['IDE releases source, build and code allocations before exit']=data('ide-exit',kernel['_page_owners'],32).count(2)==3
        checks['actual source screen still contains unsaved text']=all(line.decode()in screenshots['source-retained']for line in (prefix.rstrip(),source.splitlines()[0],source.splitlines()[2]))
    folder=n.RUN/'ide-execute/hostfs'
    checks['ordinary child executed in original directory']=(folder/'XCHILD.TXT').read_bytes()==b'H:/'if(folder/'XCHILD.TXT').exists()else False
    checks['source and installation files unchanged; no implicit save or Pascal extension']=all((folder/path).read_bytes()==content for path,content in fixtures.items())and not(folder/'external.PAS').exists()and not(folder/'project/main.COM').exists()
    checks['all qualification inputs unchanged']=all(n.sha(Path(p))==h for p,h in hashes.items())
    report={'passed':all(checks.values()),'scope':__doc__+' Full IDE/library and hardware conformance remain open.',
        'checks':checks,'screens':screenshots,'capture_directory':str(n.RUN.relative_to(ROOT)),
        'input_sha256':hashes,'checkpoints':labels}
    (n.RUN/'ide-execute-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':main()
