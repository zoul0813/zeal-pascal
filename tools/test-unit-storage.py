#!/usr/bin/env python3
"""Native large-unit storage, code limits, source-free execution and recovery."""
import importlib.util
import json
from pathlib import Path
import re
import struct
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
WORD=lambda data,at:struct.unpack_from('<H',data,at)[0]
def groups(token,count,separator):
    per_line=127//(len(token)+len(separator));items=[token]*count
    return '\n'.join(separator.join(items[at:at+per_line])+(separator if at+per_line<count else '')for at in range(0,count,per_line))
def inline(count):
    return 'Inline(\n'+groups('>0',count//2,'/')+('/0'if count%2 else '')+'\n)'
def high(name='High',padding=15000):
    return ('unit '+name+';interface function Tail:Word;implementation uses Base;\n'
        "const Blob:array[0..139]of String[119]=(\n"+",\n".join("'"+"x"*119+"'"for _ in range(140))+"\n);\nvar V:Word;\n"
        'function Bump(X:Word):Word;begin Bump:=X end;\n'
        'procedure Padding;begin\n'+'V:=V+1;\n'*400+inline(padding-7220)+' end;\n'
        'function Tail:Word;begin Tail:=Bump(Base.F)end;end.')
def paged():
    constants='\n'.join("S%03d='%s';"%(i,'x'*96)for i in range(128))
    return ('unit Paged;interface\nconst\n'+constants+"\nW=65535;L=$12345678;R=1.5;\n"
        'function Check(X:Word):Word;implementation var V:Word;\n'
        'procedure Padding;begin\n'+'V:=V+1;\n'*700+'end;\n'
        'function Check(X:Word):Word;begin Check:=X+1 end;end.')
BASE=b'unit Base;interface function F:Word;implementation function F:Word;begin F:=37 end;end.'
def source(text):
    data=text.encode();assert len(data)<=65534 and max(map(len,data.splitlines()))<=127
    return data

def resized(artifact,length,name):
    data=bytearray(artifact);delta=length-len(data);assert delta>=0
    blob=WORD(data,26);count=WORD(data,blob+6);last=blob+16+8*(count-1)
    assert WORD(data,34) and WORD(data,last+6)==0 and len(name)==data[18]
    data[48:48+len(name)]=name.encode();data.extend(bytes(delta))
    for at,value in [(6,length),(34,WORD(data,34)+delta),(blob+10,WORD(data,blob+10)+delta),(last+2,WORD(data,last+2)+delta)]:
        assert value<=65535;struct.pack_into('<H',data,at,value)
    checksum=2166136261
    for i,value in enumerate(data):
        if not 28<=i<32:checksum=((checksum^value)*16777619)&0xffffffff
    struct.pack_into('<I',data,28,checksum);return bytes(data)

def fault_run(kind,symbols,artifacts):
    observed={};target='zzvbtest'
    class FaultConsole(n.DebugConsole):
        def command(self,command,capture=False):
            if command!='@late-failure':return super().command(command,capture)
            call=lambda value:super(FaultConsole,self).command(value)
            def bytes_at(address,count):
                state=n.memory(call(f'rb 0x{address:x} {count}'))
                return bytes(state.get(address+i,0)for i in range(count))
            def ready():
                pages=bytes_at(symbols['_code_pages'],18)
                return 32<=pages[16]<64 and pages[17]==255
            def seek(entry,predicate):
                call(f'bp 0x{entry:x}')
                for _ in range(2000):
                    call('run 2000000000');regs=call('regs')
                    if f'PC={entry:04x} 'in regs and predicate(regs):break
                    call('s')
                else:raise RuntimeError('late-prefix fault boundary not reached')
                call(f'bc 0x{entry:x}')
            def write_at(address,data):
                # The emulator console accepts at most six payload bytes per wb.
                for at in range(0,len(data),6):
                    chunk=data[at:at+6]
                    call('wb 0x%x %s'%(address+at,' '.join('0x%02x'%v for v in chunk)))
                if bytes_at(address,len(data))!=data:
                    raise RuntimeError('fault patch write did not round-trip')
            page_new=symbols['_page_new']
            seek(page_new,lambda regs:ready())
            if kind=='allocation':
                entry=symbols['_test_exhaust_pages'];relay=symbols['___sdcc_bcall_ehl']
                stub=bytes((0x1e,entry>>16,0x21,entry&255,(entry>>8)&255,
                    0xcd,relay&255,relay>>8,0x3a,symbols['_test_fault_error']&255,symbols['_test_fault_error']>>8,
                    0x32,symbols['_platform_error']&255,symbols['_platform_error']>>8,0x3e,0xff,0xc9))
                original=bytes_at(page_new,len(stub));observed['patched_page_new_bytes']=len(stub)
                write_at(page_new,stub)
                fail=symbols['_cc_fail']&65535
                def front(regs):
                    mapping=call('mmu');page=bytes_at(symbols['_code_pages']+3,1)[0]
                    return bool(re.search(r'8000 -> 0*'+f'{page*16384:x}'+r'\b',mapping))
                seek(fail,front)
                observed['held_pages']=bytes_at(symbols['_test_fault_count'],1)[0]
                observed['allocation_error']=bytes_at(symbols['_test_fault_error'],1)[0]
                write_at(page_new,original)
                observed['patch_restored']=bytes_at(page_new,len(original))==original
            else:
                call('s')
                def prefix(regs):
                    match=re.search(r'DE=([0-9a-fA-F]+)',regs)
                    return ready()and match and int(match[1],16)==16384
                seek(symbols['_buffer_set'],prefix)
                call('press CTRL');call('tap U');call('release CTRL')
                observed['cancelled_after_second_prefix_allocation']=True
            call('s');return 'late-prefix fault injected\n'
    fixtures=dict(artifacts)
    fixtures.update({'STORAGE.RUN':b'1','PAGED.PAS':source(paged()),'BASE.PAS':BASE,
        'ALLOC.RUN'if kind=='allocation'else'CANCEL.RUN':b'1'})
    commands=['run 100000000']+n.program_launch(target)+['@late-failure',f"bp 0x{symbols['_test_stop']:x}",'run 120000000000','s','regs','rb 0x4000 32']
    for name in('_test_done','_test_passed','_test_failed','_test_index','_test_storage_count','_test_stack_low','_test_heap_base'):
        commands.append(f'rb 0x{symbols[name]:x} 2')
    commands += [f"rb 0x{symbols['_test_storage']:x} 24",f"rb 0x{symbols['_test_failures']:x} 128",'quit']
    raw=n.execute('unit-storage-'+kind,commands,fixtures,timeout=3600,console_class=FaultConsole)
    frame=re.split(r'\[DBG\] PC=',raw)[-1];state=n.memory(frame)
    word=lambda at:state.get(at,0)|(state.get(at+1,0)<<8)
    value=lambda name:word(symbols[name])
    error=word(symbols['_test_storage']+6);retry=word(symbols['_test_storage']+18)
    checks={'authenticated completion':frame.startswith('fc01 ')and state.get(symbols['_test_done'])==255 and bytes(state.get(0x4000+i,0)for i in range(32))==fixtures[target+'.bin'][:32],
        'cleanup and fresh compilation pass':value('_test_storage_count')==2 and value('_test_passed')==value('_test_index')and value('_test_failed')==0,
        'expected failure then successful retry':error==(99 if kind=='allocation'else 255)and retry==0}
    if kind=='allocation':checks['fault patch restored and primary error retained']=observed.get('patch_restored')and observed.get('allocation_error',0)!=0 and word(symbols['_test_storage']+10)==observed['allocation_error']
    return {'passed':all(checks.values()),'checks':checks,'observed':observed,'assertions':value('_test_passed'),
        'failure_indices':[word(symbols['_test_failures']+2*i)for i in range(min(value('_test_failed'),64))],
        'capture_directory':str(n.RUN.relative_to(ROOT))}

def main():
    target='zzvbtest';symbols=n.symbols(target);observed={};checks={}
    class StorageConsole(n.DebugConsole):
        def checkpoint(self,stage):
            call=lambda value:super(StorageConsole,self).command(value,True)
            entry=symbols['_storage_checkpoint']&65535
            call(f'bp 0x{entry:x}')
            for _ in range(1000):
                call('run 2000000000');regs=call('regs');mapping=call('mmu')
                page=n.memory(call(f"rb 0x{symbols['_code_pages']:x} 1"))[symbols['_code_pages']]
                state=n.memory(call(f"rb 0x{symbols['_test_stage']:x} 2"))
                actual=state.get(symbols['_test_stage'],0)|(state.get(symbols['_test_stage']+1,0)<<8)
                if f'PC={entry:04x} 'in regs and actual==stage and re.search(r'8000 -> 0*'+f'{page*16384:x}'+r'\b',mapping):break
                call('s')
            else:raise RuntimeError('storage checkpoint not reached')
            call(f'bc 0x{entry:x}');return call
        def command(self,command,capture=False):
            if command=='@storage-boundaries':
                call=self.checkpoint(49);disk=self.directory/'hostfs'
                for field,count in (('_test_storage',60),('_test_failures',128),('_test_diagnostics',256),('_test_failed',2),('_test_index',2),('_test_passed',2),('_test_storage_count',2)):
                    call(f'rb 0x{symbols[field]:x} {count}')
                artifact=(disk/'PAGED.ZPU').read_bytes();high_bytes=(disk/'HIGH.ZPU').read_bytes()
                assert len(artifact)>=48 and len(high_bytes)>=48,'boundary unit authoring failed; see checkpoint diagnostics'
                blob=WORD(artifact,26);fragments=WORD(artifact,blob+6);references=WORD(artifact,blob+8)
                observed['paged_bytes']=len(artifact);observed['prefix_bytes']=blob+16+8*fragments+8*references
                checks['prefix and module cross physical pages']=observed['prefix_bytes']>16384 and len(artifact)>32768
                # Every data reference and PC/export/fragment record occupied
                # a catalog node; exclude the ordinary relocation ledger.
                maps=WORD(artifact,24);pcs=WORD(artifact,22);exports=WORD(artifact,20)
                ordinary=n.memory(call(f"rb 0x{symbols['_test_storage']+16:x} 2"));ordinary=ordinary.get(symbols['_test_storage']+16,0)|(ordinary.get(symbols['_test_storage']+17,0)<<8)
                checks['catalog necessarily crosses a physical page']=256+12*(pcs+exports+fragments-1+references-ordinary)>16384
                maximum=15000+32766-WORD(high_bytes,32)
                (disk/'LAST.PAS').write_bytes(source(high('Last',maximum)))
                (disk/'OVERCODE.PAS').write_bytes(source(high('Overcode',maximum+1)))
                (disk/'LIMIT.ZPU').write_bytes(resized(artifact,65470,'limit'))
                (disk/'OVERS.ZPU').write_bytes(resized(artifact,65470,'overs')+b'\0')
                observed['high_code_bytes']=WORD(high_bytes,32)
                checks['unit code above executable limit']=WORD(high_bytes,32)>29696
                call('s');return 'built exact boundary fixtures from authenticated native artifacts\n'
            if command=='@remove-libraries':
                call=self.checkpoint(60);disk=self.directory/'hostfs'
                for path in disk.glob('*.ZPU'):path.unlink()
                for name in ('BASE','PAGED','HIGH','LAST'):
                    (disk/(name+'.PAS')).unlink(missing_ok=True)
                observed['libraries_deleted']=True;call('s');return 'removed unit source and library fixtures before saved execution\n'
            return super().command(command,capture)
    artifacts={target+'.'+ext:(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures=dict(artifacts)
    fixtures.update({'STORAGE.RUN':b'1','BASE.PAS':BASE,'PAGED.PAS':source(paged()),'HIGH.PAS':source(high()),
        'PGCLIENT.PAS':source("uses Paged;var Z:Integer;begin Z:=0;\nif(Check(42)<>43)or(W<>65535)or(L<>$12345678)or(R<>1.5)then Z:=1 div Z;\nif S127<>'"+'x'*96+"'then Z:=1 div Z end."),
        'HICLIENT.PAS':b'uses High;var Z:Integer;begin Z:=0;if Tail<>37 then Z:=1 div Z end.',
        'LACLIENT.PAS':b'uses Last;var Z:Integer;begin Z:=0;if Tail<>37 then Z:=1 div Z end.',
        'BOUND.PAS':b'uses Limit;begin end.', 'OVERFILE.PAS':b'uses Overs;begin end.',
        'FAILBIG.PAS':source('const Blob:array[0..14999]of Byte=(\n'+groups('0',15000,',')+'\n);\nprocedure P;begin '+inline(15000)+' end;begin P end.')})
    commands=['run 100000000']+n.program_launch(target)+['@storage-boundaries','@remove-libraries',f"bp 0x{symbols['_test_stop']:x}",'run 120000000000','s','regs','rb 0x4000 32']
    fields=('_test_done','_test_passed','_test_failed','_test_index','_test_stage','_test_storage_count','_test_client_count','_test_stack_low','_test_heap_base')
    commands += [f'rb 0x{symbols[name]:x} 2'for name in fields]
    commands += [f"rb 0x{symbols['_test_storage']:x} 60",f"rb 0x{symbols['_test_clients']:x} 192",f"rb 0x{symbols['_test_failures']:x} 128",f"rb 0x{symbols['_test_diagnostics']:x} 256",'quit']
    raw=n.execute('unit-storage',commands,fixtures,timeout=3600,console_class=StorageConsole)
    frame=re.split(r'\[DBG\] PC=',raw)[-1];state=n.memory(frame)
    word=lambda name:state.get(symbols[name],0)|(state.get(symbols[name]+1,0)<<8)
    checks.update({'authenticated native completion':frame.startswith('fc01 ')and state.get(symbols['_test_done'])==255 and bytes(state.get(0x4000+i,0)for i in range(32))==fixtures[target+'.bin'][:32],
        'all assertions pass':word('_test_passed')==word('_test_index')and word('_test_failed')==0,
        'all boundary authors and clients run':word('_test_storage_count')==5 and word('_test_client_count')==4,
        'libraries removed before saved execution':observed.get('libraries_deleted')==True})
    authors=[]
    for index,name in enumerate(('BASE','PAGED','HIGH','LAST','OVERCODE')):
        values=[]
        for field in range(6):
            at=symbols['_test_storage']+index*12+field*2;values.append(state.get(at,0)|(state.get(at+1,0)<<8))
        authors.append(dict(zip(('code_bytes','maps','relocations','error','position','platform_error'),values),name=name))
    checks['exact upper code boundary']=authors[3]['code_bytes']==32766 and authors[3]['error']==0 and authors[4]['error']==99
    report={'passed':all(checks.values()),'checks':checks,'observed':observed,'authors':authors,'assertions':word('_test_passed'),
        'failures':[state.get(symbols['_test_failures']+2*i,0)|(state.get(symbols['_test_failures']+2*i+1,0)<<8)for i in range(min(word('_test_failed'),64))],
        'diagnostics':[state.get(symbols['_test_diagnostics']+2*i,0)|(state.get(symbols['_test_diagnostics']+2*i+1,0)<<8)for i in range(min(2*word('_test_failed'),128))],
        'stack_margin':word('_test_stack_low')-word('_test_heap_base'),'capture_directory':str(n.RUN.relative_to(ROOT))}
    report['boundaries_passed']=report['passed'];report['passed']=False
    report['phase']='checking failure recovery' if report['boundaries_passed'] else 'boundary failure'
    positive=n.RUN/'unit-storage-report.json';positive.write_text(json.dumps(report,indent=2)+'\n')
    if report['boundaries_passed']:
        report['faults']={kind:fault_run(kind,symbols,artifacts)for kind in ('allocation','cancellation')}
        report['passed']=all(result['passed']for result in report['faults'].values())
        report['phase']='complete'
        positive.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
