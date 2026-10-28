#!/usr/bin/env python3
"""Native logged-drive, directory masks/paging and preserved IDE source state."""
from pathlib import Path
import gzip
import hashlib
import importlib.util
import json
import re

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)


def main():
    n.SHIFTED['?']='SLASH'
    sym=n.symbols('zpascal');kernel=n.kernel_symbols();key=sym['_key_read'];clear=kernel['_video_vram_set']
    work,build,editor=(sym[name]for name in ('_work','_build','_editor'))
    source=b"program Keep;\nvar n:integer;\nbegin n:=7;writeln('DIR-CACHE-OK')end.\n";prefix=b'{unsaved}\n'
    fixtures={'project/main.pas':source,'card-lower.pas':b'lower\n','card-upper.pas':b'upper\n','readme.txt':b'note\n'}
    fixtures.update({'masks/'+name:b'x' for name in ('a1.pas','aB.pas','a12.pas','noext','Multi.part.pas','Caps.PAS','star*file.pas','12345678901.pas')})
    fixtures['masks/Sub.pas/keep']=b'kept';fixtures.update({f'many/f{i:02}.pas':b'x'for i in range(75)})
    template=gzip.decompress(n.TF_TEMPLATE.read_bytes());setup=['run 100000000']
    for command in ('H:/mkdir T:/listdir','H:/mkdir T:/listdir/sub',
        'H:/cp H:/card-lower.pas T:/listdir/card.pas','H:/cp H:/card-upper.pas T:/listdir/CARD.PAS',
        'H:/cp H:/readme.txt T:/listdir/readme.txt'):
        setup+=n.type_text(command+'\n',ticks=2000000)+['run 1000000000']
    setup+=['quit']
    extra={name:(n.COREUTILS/name).read_bytes()for name in ('mkdir','cp')}
    n.execute('directory-card-setup',setup,{**fixtures,**extra},tf_image=template,eeprom_image=n.blank_eeprom(),timeout=180)
    card=(n.RUN/'directory-card-setup/tf.img').read_bytes();card_files=n.zealfs_files(card)
    assert all(card_files.get(path)==data for path,data in {
        '/listdir/card.pas':b'lower\n','/listdir/CARD.PAS':b'upper\n','/listdir/readme.txt':b'note\n'}.items())
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py',n.EMULATOR,n.ROM,n.TF_TEMPLATE]
    inputs+=list((ROOT/'src').glob('*'))+list((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/zpascal.{ext}'for ext in ('bin','zpl','map','banks.json')]
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    labels=[];typed=lambda text:n.type_text(text,ticks=100000000)
    editing=lambda text:n.type_text(text,ticks=100000000)
    control=lambda letter:n.control(letter)[:-1]+['run 100000000']
    def state(label,trigger,cycles=4000000000):
        labels.append((label,'state'))
        if isinstance(trigger,str):trigger=['tap '+trigger]
        assert all(not part.startswith('run ')for part in trigger)
        return [f'bp 0x{key:x}']+trigger+[f'run {cycles}','regs','mmu',
            f"rb 0x{sym['_ide_state']:x} 1",f'rb 0x{work:x} 140',f'rb 0x{build:x} {n.BUILD_BYTES}',
            f'rb 0x{editor:x} 223',f"rb 0x{sym['_main_file']:x} 128",f"rb 0x{sym['_buffers']:x} {n.SOURCES_BYTES}",
            f"rb 0x{sym['_ram_result']:x} 128",f'bc 0x{key:x}']
    def screen(label,trigger,rows=16):
        labels.append((label,'screen'))
        if isinstance(trigger,str):trigger=['tap '+trigger]
        assert all(not part.startswith('run ')for part in trigger)
        result=[f'bp 0x{clear:x}']+trigger+['run 4000000000','regs','mmu']
        for offset in range(0,rows*80,256):result+=[f'rb 0x{0x4000+offset:x} {min(256,rows*80-offset)}']
        for symbol,size in (('_vfs_current_dir',128),('_dev_table',32),('_page_owners',32),('mmu_bitmap',4)):
            result+=[f'rb 0x{kernel[symbol]:x} {size}']
        return result+[f'bc 0x{clear:x}','run 400000000']
    commands=['run 100000000']+typed('cd H:/\n')+['run 100000000']+n.program_launch()+['run 10000000']
    commands+=typed('nwH:/project/main.pas\ne')+editing(prefix.decode())+control('K')+typed('b')+control('K')+typed('d')
    commands+=typed('mH:/project/main.pas\n')+state('baseline','C')+screen('baseline-menu','SPACE')
    commands+=typed('lt')+state('logged-t','ENTER')+screen('t-menu','SPACE')
    for label,value in [('missing-drive','z'),('invalid-drive','H:/project')]:
        commands+=typed('l'+value)+state(label,'ENTER')+screen(label+'-screen','ESC')
    commands+=typed('l')+state('empty-drive','ENTER',100000000)
    commands+=typed('l')+state('canceled-drive','ESC',100000000)
    commands+=typed('l')+state('interrupted-drive',control('U')[:-1],100000000)
    for label,value in [('tf-lower','T:/listdir/*.pas'),('tf-upper','T:/listdir/*.PAS'),('tf-directory','T:/listdir/'),('empty-mask','')]:
        commands+=typed('d'+value)+state(label,'ENTER')+screen(label+'-screen','ESC')
    commands+=typed('lh:')+state('logged-h','ENTER')+screen('h-menu','SPACE')
    for label,value in [('single','project/./../masks/a?.pas'),('backtrack','masks/*part*as'),
        ('exact','masks/noext'),('all','masks/*'),('no-match','masks/z*.pas'),
        ('missing-directory','not-created/'),('wild-parent','ma*/a?.pas'),('long-component','1234567890123456/')]:
        commands+=typed('d'+value)+state(label,'ENTER')+screen(label+'-screen','ESC')
    commands+=typed('d')+state('canceled-directory','ESC',100000000)
    commands+=typed('d')+state('interrupted-directory',control('U')[:-1],100000000)
    commands+=typed('dH:/many/')+state('first-pager','ENTER')+screen('page-one','SPACE',40)+screen('page-two','ENTER',40)+screen('page-three','ESC',40)
    commands+=typed('dH:/many')+state('abort-pager','ENTER')+screen('aborted-page','ESC',40)
    commands+=state('after-paging','SPACE',100000000)
    commands+=typed('lB:/')+state('logged-b','ENTER')+screen('b-menu','SPACE')
    commands+=typed('d')+state('empty-volume','ENTER')+screen('empty-volume-screen','ESC')
    commands+=typed('lA')+state('logged-a','ENTER')+screen('a-menu','SPACE')
    commands+=state('cached-run','R')+['tap ESC','run 100000000']
    commands+=typed('e')+screen('source-retained',control('Q')[:-1],8)+typed('?')+control('K')+typed('d')
    labels.append(('ide-exit','exit'))
    commands+=[f"bp 0x{kernel['zos_loader_exit']:x}"]+typed('qn')+['regs','mmu',f"rb 0x{kernel['_page_owners']:x} 32",'quit']
    raw=n.execute('ide-directory',commands,fixtures,tf_image=card,eeprom_image=n.blank_eeprom(),timeout=900)
    parts=re.split(r'\[DBG\] PC=',raw)[1:];checks={'all native directory checkpoints reached':len(parts)==len(labels)}
    screens={}
    if len(parts)==len(labels):
        frames={label:(n.memory(part),part)for(label,_),part in zip(labels,parts)}
        data=lambda label,address,size:bytes(frames[label][0].get(address+i,0)for i in range(size))
        text=lambda label,address:data(label,address,128).split(b'\0')[0]
        checks['state checkpoints stop at actual resident reader']=all(part.startswith(f'{key:04x} ')for(_,kind),part in zip(labels,parts)if kind=='state')
        baseline={address:data('baseline',address,size)for address,size in ((work,140),(build,n.BUILD_BYTES),(editor,223),(sym['_main_file'],128),(sym['_buffers'],n.SOURCES_BYTES))}
        checks['all drive/listing success, failure, cancellation and paging preserve complete editor/build/catalog state']=all(
            data(label,address,len(expected))==expected for label,kind in labels if kind=='state'and label!='cached-run'for address,expected in baseline.items())
        checks['baseline contains unsaved source, a block mark and valid compiled snapshot']=baseline[work][1]==1 and baseline[editor][152]==1 and baseline[build][4]==1 and baseline[build][51:83]==hashlib.sha256(prefix+source).digest()
        for label,kind in labels:
            if kind!='screen':continue
            m,part=frames[label];size=sum(1 for address in m if 0x4000<=address<0x4c80)
            assert size%80==0,(label,size)
            screens[label]=[data(label,0x4000+offset,80).replace(b'\0',b' ').decode('ascii',errors='replace').rstrip()for offset in range(0,size,80)]
            checks[label+' inspects actual character VRAM']=part.startswith(f'{clear:04x} ')and '4000 -> 100000' in part
        listing=lambda label:[row for row in screens[label][2:]if row and row!='No matching entries'and not row.startswith(('Free space unavailable','Press Escape','More:'))]
        expected_all={'a1.pas','aB.pas','a12.pas','noext','Multi.part.pas','Caps.PAS','star*file.pas','12345678901.pas','Sub.pas/'}
        checks['native TF masks retain case']=listing('tf-lower-screen')==['card.pas']and listing('tf-upper-screen')==['CARD.PAS']
        checks['explicit directory lists files and marks subdirectories']=set(listing('tf-directory-screen'))=={'card.pas','CARD.PAS','readme.txt','sub/'}
        checks['empty mask lists current logged root']=listing('empty-mask-screen')==['listdir/']
        checks['relative masks normalize dot/parent and question mark matches one byte']=set(listing('single-screen'))=={'a1.pas','aB.pas'}
        checks['star backtracking spans native dots']=listing('backtrack-screen')==['Multi.part.pas']
        checks['exact extensionless mask does not add PAS']=listing('exact-screen')==['noext']
        checks['all-name mask covers extensionless, long, literal-star and directory names']=set(listing('all-screen'))==expected_all
        checks['unmatched masks and empty volume are explicit']=all('No matching entries'in screens[label]for label in ('no-match-screen','empty-volume-screen'))
        checks['missing/invalid directory paths report errors']=all(any('Directory failed, native error 'in row for row in screens[label+'-screen'])for label in ('missing-directory','wild-parent','long-component'))
        checks['missing/invalid drives report errors']=all(any('Logged drive failed, native error 'in row for row in screens[label+'-screen'])for label in ('missing-drive','invalid-drive'))
        checks['free-capacity absence is honest']=all('Free space unavailable'in screens[label+'-screen']for label in ('tf-directory','single','no-match','empty-volume'))
        page_names=[listing(label)for label in ('page-one','page-two','page-three')]
        checks['pagination displays all 75 names exactly once']=list(map(len,page_names))==[35,35,5]and len({name for page in page_names for name in page})==75 and set(sum(page_names,[]))=={f'f{i:02}.pas'for i in range(75)}
        expected_dirs={'baseline-menu':b'H:/','t-menu':b'T:/','missing-drive-screen':b'T:/','invalid-drive-screen':b'T:/','h-menu':b'H:/','b-menu':b'B:/','a-menu':b'A:/'}
        checks['logged roots and failed selections preserve actual native directory']=all(text(label,kernel['_vfs_current_dir'])==path for label,path in expected_dirs.items())
        checks['menu displays actual logged root']=all(any('Directory: '+path.decode()in row for row in screens[label])for label,path in expected_dirs.items()if label.endswith('menu'))
        checks['empty/escaped/interrupted fields and pager abort return to menu']=all(data(label,sym['_ide_state'],1)==b'\x01'for label in ('empty-drive','canceled-drive','interrupted-drive','canceled-directory','interrupted-directory','after-paging'))
        closed=[label for label,kind in labels if kind=='screen'and label not in ('page-one','page-two')]
        checks['directory descriptors close on EOF, errors and pager cancellation']=all(data(label,kernel['_dev_table'],32)==data('baseline-menu',kernel['_dev_table'],32)for label in closed)
        checks['page and allocator ownership remains unchanged']=all(data(label,kernel['_page_owners'],32)==data('baseline-menu',kernel['_page_owners'],32)and data(label,kernel['mmu_bitmap'],4)==data('baseline-menu',kernel['mmu_bitmap'],4)for label,kind in labels if kind=='screen')
        checks['R after logged-drive changes reuses valid absolute-root cache']=data('cached-run',build,n.BUILD_BYTES)==baseline[build]and data('cached-run',sym['_ram_result']+26,1)==b'\x01'
        checks['actual source display retains unsaved comment and original program']=all(any(line.decode()in row for row in screens['source-retained'])for line in (prefix.rstrip(),source.splitlines()[0],source.splitlines()[2]))
        checks['clean IDE exit releases all non-loader pages']=frames['ide-exit'][1].startswith(f"{kernel['zos_loader_exit']:04x} ")and data('ide-exit',kernel['_page_owners'],32).count(2)==3
    folder=n.RUN/'ide-directory'
    checks['directory and drive commands never write TF or EEPROM']=(folder/'tf.img').read_bytes()==card and(folder/'eeprom.img').read_bytes()==n.blank_eeprom()
    checks['host files unchanged and missing directory never created']=all((folder/'hostfs'/path).read_bytes()==content for path,content in fixtures.items())and not(folder/'hostfs/not-created').exists()and not(folder/'hostfs/project/main.COM').exists()
    checks['all qualification inputs unchanged']=all(n.sha(Path(p))==h for p,h in hashes.items())
    report={'passed':all(checks.values()),'scope':__doc__+' Bounded native emulator acceptance; full IDE/language and hardware conformance remain open.',
        'checks':checks,'screens':screens,'checkpoints':labels,'capture_directory':str(n.RUN.relative_to(ROOT)),
        'prepared_tf_sha256':hashlib.sha256(card).hexdigest(),'input_sha256':hashes}
    (n.RUN/'ide-directory-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':main()
