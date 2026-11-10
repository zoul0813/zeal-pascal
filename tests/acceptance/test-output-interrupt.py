#!/usr/bin/env python3
"""Physical Ctrl-U during direct IDE saves, with retained source and disk-error contracts."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
# Keep source and emitted map large enough for a physical Ctrl-U sequence
# to arrive while the backup copy or output write is still in progress.
SOURCE=(
    b'program CancelSave;\n'+(b'{'+b'source '*10+b'}\n')*100+b'begin\n'+b'writeln(7);\n'*100+b'end.\n')
PREFIX=b'{unsaved}\n'


def run_case(kind,helper,ordinal,typing=False,drive="H"):
    sym=n.symbols('zpascal');kernel=n.kernel_symbols();key=sym['_key_read'];poll=sym['_key_interrupt'];clear=kernel['keyboard_next_key']
    work,build,editor=sym['_work'],sym['_build'],sym['_editor']
    # Read the target getter's absolute load operand, avoiding a new product
    # global solely for observation. Bind this probe to the packaged library.
    library=(ROOT/'build/zpascal.zpl').read_bytes();count=library[5]
    sizes=struct.unpack_from('<'+'H'*count,library,16)
    at=16+2*count+sum(sizes[:12])+(sym['_output_interrupt_reason']&0xffff)-0x8000
    assert library[at]==0x3a and library[at+3]==0xc9
    reason=int.from_bytes(library[at+1:at+3],'little')
    entry=sym['_save_source'] if kind=='source' else sym['_save_build']
    checkpoint=sym[helper]&0xffff
    fixtures={'project/main.pas':SOURCE,'project/main.BAK':b'prior backup\n',
              'project/main.COM':b'previous executable\n'*15,'project/main.ZMP':b'previous map\n'*23}
    labels=[]
    def probe(label,pc):
        labels.append((label,pc))
        return ['regs','mmu',f'rb 0x{work:x} 140',f'rb 0x{build:x} {n.BUILD_BYTES}',f'rb 0x{editor:x} 223',
                f"rb 0x{sym['_main_file']:x} 128",f"rb 0x{sym['_code_pages']:x} 16",
                f"rb 0x{reason:x} 1",f"rb 0x{sym['_platform_error']:x} 1",
                f"rb 0x{sym['_console_scratch']:x} 256",f"rb 0x{kernel['_page_owners']:x} 32"]
    def state(label,trigger):
        return trigger+[n.ImageStop(key,'zpascal.bin')]+probe(label,key)[1:]
    typed=lambda text:n.type_text(text,ticks=100000000)
    commands=['run 100000000']
    if drive=='T':
        commands+=n.type_text('H:/mkdir T:/project\n',ticks=2000000)+['run 100000000']
        for path in fixtures:
            commands+=n.type_text('H:/cp H:/'+path+' T:/'+path+'\n',ticks=2000000)+['run 1000000000']
    commands+=n.program_launch()+['run 10000000']
    commands+=typed(f'nw{drive}:/project/main.pas\nm{drive}:/project/main.pas\ne'+PREFIX.decode())+n.control('K')+typed('d')
    if kind!='source':commands+=typed('ocq')
    commands+=[f'bp 0x{clear:x}','tap ENTER','@kernel-clear']+probe('baseline-kernel',clear)+[f'bc 0x{clear:x}']
    commands+=state('baseline',['s','tap SPACE'])+['run 100000000']
    # Stop in the resident API after the compiler is finished. Bank-window
    # breakpoints are armed only after entering this publication operation.
    commands+=[f'bp 0x{entry:x}','tap '+('S' if kind=='source' else 'C'),'run 4000000000']+probe('entry',entry)
    commands+=[f'bc 0x{entry:x}',f'bp 0x{checkpoint:x}','s','run 4000000000']
    for _ in range(ordinal-1):commands+=['s','run 4000000000']
    commands+=probe('phase',checkpoint)+[f'bc 0x{checkpoint:x}',f'bp 0x{poll:x}','s','run 4000000000']
    # One complete transfer chunk is allowed before sending Ctrl-U at the next
    # polling boundary. This exercises cancellation after a partial destination write.
    commands+=['s','run 4000000000']+probe('partial',poll)+[f'bc 0x{poll:x}','s']
    if typing:commands+=['tap E','tap Z']
    commands+=['press CTRL','tap U','release CTRL']+state('cancelled',[])
    # Capture kernel ownership as the acknowledgment reads its fresh Escape.
    commands+=[f'bp 0x{clear:x}','tap ESC']
    commands+=['@kernel-clear']+probe('screen',clear)
    commands += [f'bc 0x{clear:x}']
    if typing:
        commands+=state('typing-pending',[])+state('typing-editor',[])+state('typing-inserted',[])
        commands+=typed('\b')+n.control('K')+typed('d')
    else:commands+=['run 100000000']
    # Stop before retry so the independent disk oracle can prove that the cancellation contract holds. Retrying is exercised separately by
    # the host fault suite and the repeated native publication suite.
    commands+=['quit']
    name=f'output-interrupt-{drive}-{kind}-{helper.strip("_")}-{ordinal}'+('-typing' if typing else '')
    media=n.prepare_media() if drive=='T' else None
    launch_files=dict(fixtures)
    if drive=='T':launch_files.update({name:(n.COREUTILS/name).read_bytes()for name in ('mkdir','cp')})
    class PublicationConsole(n.DebugConsole):
        def command(self,command,capture=False):
            if command!='@kernel-clear':return super().command(command,capture)
            for _ in range(1000):
                super().command('run 2000000000')
                frame=super().command('regs')
                if f'PC={clear:04x} ' in frame:return 'kernel keyboard reached\n'
                super().command('s')
            raise RuntimeError('kernel keyboard checkpoint not reached')
    raw=n.execute(name,commands,launch_files,tf_image=media,timeout=900,console_class=PublicationConsole)
    parts=re.split(r'\[DBG\] PC=',raw)[1:]
    checks={'all expected checkpoints reached':len(parts)==len(labels)}
    if len(parts)==len(labels):
        frames={label:(n.memory(part),part)for (label,_),part in zip(labels,parts)}
        data=lambda label,at,size:bytes(frames[label][0].get(at+i,0)for i in range(size))
        checks['all checkpoints at expected native entry']=all(part.startswith(f'{pc:04x} ')for (_,pc),part in zip(labels,parts))
        page=data('phase',sym['_code_pages']+12,1)[0]
        checks['transfer breakpoint belongs to actual transport bank']=f'8000 -> {page*16384:06x}' in frames['phase'][1]
        checks['cancellation releases the actual kernel-owned temporary pages']=(
            data('baseline-kernel',kernel['_page_owners'],32)==data('screen',kernel['_page_owners'],32) and
            re.search(r'c000 -> ([0-9a-fA-F]+)',frames['baseline-kernel'][1])[1]==re.search(r'c000 -> ([0-9a-fA-F]+)',frames['screen'][1])[1])
        checks['Ctrl-U is a private cancellation outcome']=data('cancelled',reason,1)==b'\1'
        for address,size,label in [(work,140,'dirty source and cursor'),(editor,223,'editor state'),(sym['_main_file'],128,'Main identity')]:
            checks['cancellation retains '+label]=data('baseline',address,size)==data('cancelled',address,size)
        if kind!='source':checks['cancelled publication discards executable cache']=data('cancelled',build,5)==b'\xff\xff\xff\xff\0'
        if typing:
            word=lambda label,at:int.from_bytes(data(label,at,2),'little')
            checks['fresh Escape preserves ordinary typing for the editor']=(word('typing-pending',work+2)==len(PREFIX+SOURCE) and
                word('typing-editor',work+2)==len(PREFIX+SOURCE) and word('typing-inserted',work+2)==len(PREFIX+SOURCE)+1)
    disk=n.RUN/name/'hostfs'
    if drive=='T':
        files=n.zealfs_files((n.RUN/name/'tf.img').read_bytes())
        content=lambda path:files.get('/'+path)
        recoveries=[path for path in files if path.startswith('/project/ZP')]
        checks['HostFS fixtures preserved']=all((disk/path).read_bytes()==data for path,data in fixtures.items())
    else:
        content=lambda path:(disk/path).read_bytes() if (disk/path).is_file() else None
        recoveries=list((disk/'project').glob('ZP*'))
    if kind=='source':
        checks['previous source survives in source or BAK']=SOURCE in (content('project/main.pas'),content('project/main.BAK'))
    else:
        checks['publication leaves unrelated former sidecar untouched']=content('project/main.ZMP')==fixtures['project/main.ZMP']
    checks['no recovery transaction directory']=not recoveries
    return {'passed':all(checks.values()),'kind':kind,'drive':drive,'helper':helper,'ordinal':ordinal,'checks':checks,
            'capture':str((n.RUN/name).relative_to(ROOT))}


CASES = {
    'host-source-backup': ('source', '_output_copy_file', 1, False),
    'host-source-store-typing': ('source', '_output_store_source', 1, True),
    'host-build-executable': ('build', '_output_store_range', 1, False),
    'host-build-map': ('build', '_output_store_range', 2, False),
    'tf-source-store': ('source', '_output_store_source', 1, False, 'T'),
    'tf-build-map': ('build', '_output_store_range', 2, False, 'T'),
}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    selection=parser.add_mutually_exclusive_group()
    selection.add_argument('--quick',action='store_true')
    selection.add_argument('--case',choices=CASES)
    parser.add_argument('--list-cases',action='store_true')
    parser.add_argument('--jobs',type=int,choices=range(1,4),default=3,
                        help='Maximum concurrent emulator cases (default: 3)')
    args=parser.parse_args()
    if args.list_cases:
        print("\n".join(CASES));return
    if args.case:args.jobs=1
    inputs=[Path(__file__),ROOT/'CMakeLists.txt',ROOT/'tests/acceptance/test-native.py',n.EMULATOR,n.ROM,n.TF_TEMPLATE,n.COREUTILS/'cp',n.COREUTILS/'mkdir']
    inputs+=list((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+[ROOT/f'build/zpascal.{ext}'for ext in ('bin','zpl','map','banks.json')]
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    selected=[args.case] if args.case else list(CASES)[:2] if args.quick else list(CASES)
    cases=[CASES[name] for name in selected]
    results=[]
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for result in pool.map(lambda case:run_case(*case),cases):
            results.append(result);print(json.dumps(result),flush=True)
    report={'passed':all(r['passed']for r in results)and all(n.sha(Path(p))==h for p,h in hashes.items()),
            'scope':__doc__+' Bounded native emulator acceptance; not full conformance.',
            'results':results,'input_sha256':hashes,'selected_cases':selected,
            'complete_publication_suite':len(selected)==len(CASES)}
    n.RUN.mkdir(parents=True,exist_ok=True);(n.RUN/'output-interrupt-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'passed':report['passed'],'report':str(n.RUN/'output-interrupt-report.json')}))
    assert report['passed']


if __name__=='__main__':main()
