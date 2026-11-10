#!/usr/bin/env python3
"""Measure SYSFILE at its actual private ABI in the unmodified Zeal emulator.

This test-only placement loads every fragment so the same invocation sequence
can exercise every operation. Selective linked sizes are calculated separately
from the ZPU dependency graph; they are not claimed as complete Pascal COM sizes.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
word=lambda b,o:struct.unpack_from('<H',b,o)[0]

def catalog(unit):
    blob=word(unit,26);count=word(unit,blob+6);refs=word(unit,blob+8)
    records=[struct.unpack_from('<4H',unit,blob+16+8*i) for i in range(count)]
    references=[struct.unpack_from('<4H',unit,blob+16+8*count+8*i) for i in range(refs)]
    payload=unit[blob+16+8*count+8*refs:];exports={};at=word(unit,44)
    for _ in range(word(unit,20)):
        size=unit[at];exports[unit[at+14:at+14+size].decode()]=(word(unit,at+8),word(unit,at+10))
        at+=14+size+4*word(unit,at+6)+word(unit,at+12)
    return records,references,payload,exports


CLIENTS={
 'MIN':b'begin end.',
 'ASSIGN':b"var F:file;begin Assign(F,'H:/UNUSED.DAT')end.",
 'TYPED':b"var F:file of Integer;N,V,Z:Integer;begin Z:=0;Assign(F,'H:/DATA.DAT');Rewrite(F);for N:=1 to 8 do begin V:=N;Write(F,V)end;Close(F);Reset(F);Read(F,V);if V<>1 then Z:=1 div Z;Close(F)end.",
 'BLOCK':b"var F:file;B:array[1..256]of Byte;N,Z:Integer;begin Z:=0;for N:=1 to 256 do B[N]:=N-1;Assign(F,'H:/DATA.DAT');Rewrite(F);BlockWrite(F,B,2);Close(F);Reset(F);BlockRead(F,B,2);Close(F);for N:=1 to 256 do if B[N]<>N-1 then Z:=1 div Z end.",
 'TEXT':b"var F:Text;S:string[20];Z:Integer;begin Z:=0;Assign(F,'H:/DATA.DAT');Rewrite(F);WriteLn(F,'File text');Close(F);Reset(F);ReadLn(F,S);Close(F);if S<>'File text' then Z:=1 div Z end."
}

def clients(directory,output):
    sym=n.symbols('zpascal');rows={};kernel=n.kernel_symbols()
    bindings={name:(directory/name).read_bytes() for name in ('zpascal.bin','zpascal.zpl','SYSFILE.ZPU')}
    for name,source in CLIENTS.items():
        source=source.replace(b';',b';\n')
        typed=lambda v:n.type_text(v,ticks=100000000)
        commands=['run 100000000']+n.program_launch()+['run 100000000']+typed('n')+typed('wH:/BENCH.PAS\n')
        commands+=['tap R',n.ImageStop(sym['_key_read'],'zpascal.bin'),f"rb 0x{sym['_ram_result']:x} 128",f"rb 0x{sym['_ram_pages']:x} 3",'tap ESC','run 100000000']
        commands+=typed('ocq')+['tap C',n.ImageStop(sym['_key_read'],'zpascal.bin'),'quit']
        label='sysfile-client-'+name.lower()
        raw=n.execute(label,commands,{**bindings,'BENCH.PAS':source},timeout=600,base_programs=())
        folder=n.RUN/label
        assert not(folder/'input-error.json').exists(),folder
        memory=n.memory(raw);result=sym['_ram_result']
        assert memory.get(result+26)==1 and memory.get(result+28)==0,(name,'RAM failed')
        image=(folder/'hostfs/BENCH.COM').read_bytes()
        def remove(command,response,directory):
            if command=='run 100000000':(directory/'hostfs/SYSFILE.ZPU').unlink(missing_ok=True)
        commands=['run 100000000']+typed('H:/BENCH.COM')+[f"bp 0x{kernel['zos_loader_exit']:x}",'tap ENTER','run 2000000000','regs',f"rb 0x{kernel['_page_owners']:x} 32",'quit']
        cold=n.execute(label+'-cold',commands,{'BENCH.COM':image},timeout=180,base_programs=(),after_command=remove)
        assert f"PC={kernel['zos_loader_exit']:04x} "in cold and 'HL=000f'in cold,(name,'COM failed',cold[-1000:])
        files=n.RUN/(label+'-cold')/'hostfs'
        expected={'TYPED':b'\x08\x00\x02\x00'+struct.pack('<8H',*range(1,9)),
                  'BLOCK':bytes(range(256)),'TEXT':b'File text\r\n\x1a'}
        if name in expected:assert (files/'DATA.DAT').read_bytes()==expected[name],name
        assert not(files/'SYSFILE.ZPU').exists()
        owned=n.memory(cold)
        rows[name]={'com_bytes':len(image),'sha256':hashlib.sha256(image).hexdigest(),
            'code_bytes':word(image,22),'globals_bytes':word(image,26),'runtime_bytes':word(image,132),
            'stack_requirement_bytes':word(image,32),'execution_pages':len(set(memory[sym['_ram_pages']+i]for i in range(3))),
            'cold_execution_pages':sum(owned.get(kernel['_page_owners']+i)==2 for i in range(32)),
            'RAM_passed':True,'detached_COM_passed':True}
        print(name,rows[name],flush=True)
    report={'passed':True,'scope':'Actual production Pascal compiler, RAM and cold saved COM without Units/compiler on disk; emulator, not physical hardware.',
        'build_sha256':{name:hashlib.sha256(data).hexdigest()for name,data in bindings.items()},'programs':rows,'capture':str(n.RUN)}
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n')

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--unit',type=Path,default=ROOT/'build/SYSFILE.ZPU')
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--clients',action='store_true')
    p.add_argument('--serial',action='store_true',help='Also exercise production AUX logic with a test-only SER0 transport')
    p.add_argument('--compiler-dir',type=Path,default=ROOT/'build')
    a=p.parse_args(argv)
    if a.clients:
        clients(a.compiler_dir,a.output);return
    unit=a.unit.read_bytes()
    if a.serial:
        current=(ROOT/'build/sysfile.payload').read_bytes()
        assert unit[:8]+unit[12:28]+unit[32:]==current[:8]+current[12:28]+current[32:],'Serial probe requires the current ASM Unit'
    records,refs,payload,exports=catalog(unit)
    base=0x5000;linked=bytearray(payload)
    assert base+len(linked)<0x9000,'Benchmark placement must be revised for larger runtimes'
    for start,size,first,count in records:
        for site,module,encoded,offset in refs[first:first+count]:
            assert module==1
            target=base+records[encoded&0x3fff][0]+offset;kind=encoded>>14
            if kind==0:struct.pack_into('<H',linked,start+site,target)
            else:linked[start+site]=(target>>(8*(kind-1)))&255
    address=lambda name:base+records[exports[name][0]][0]+exports[name][1]
    handlers={1:'reset',2:'rewrite',3:'close',4:'read',5:'write',6:'seek',7:'position',8:'position',9:'position',10:'flush',11:'erase',12:'blocks',13:'blocks',14:'blocks',15:'blocks',20:'text_read',21:'text_write',22:'text_readln',23:'text_write',24:'predicate',25:'predicate',26:'predicate',27:'rename',28:'begin',30:'path',33:'path'}
    table=word(linked,address('table')-base+1)
    for op,name in handlers.items():struct.pack_into('<H',linked,table-base+2*op,address(name))
    def retained(names):
        selected=set();todo=[exports[name][0] for name in names]
        while todo:
            f=todo.pop()
            if f in selected:continue
            selected.add(f);_,_,first,count=records[f]
            todo.extend(encoded&0x3fff for _,_,encoded,_ in refs[first:first+count])
        return {'code_bytes':sum(records[f][1] for f in selected if records[f][0]<word(unit,32)),
                'workspace_bytes':sum(records[f][1] for f in selected if records[f][0]>=word(unit,32)),
                'fragments':len(selected)}
    image=bytearray(0x7000);image[:3]=b'\xc3\x80\x40';image[24:26]=struct.pack('<H',0x9000)
    image[base-0x4000:base-0x4000+len(linked)]=linked
    job=0xa100;record=0x9000;descriptor=0xa000;data=0x9400
    values=[0,record,descriptor,data,0,0,0,0xb000,0xb000,0xb000,0,0,0,0,0xf000,0xa000,0,0,0]
    image[job-0x4000:job-0x4000+38]=struct.pack('<19H',*values)
    image[descriptor-0x4000:descriptor-0x4000+3]=b'\x01\x02\x00'
    for at,text in [(0x9300,b'H:/BENCH.DAT'),(0x9340,b'H:/RENAMED.DAT'),(0x9380,b'H:/BLOCK.DAT'),(0x93c0,b'H:/TEXT.DAT')]:
        image[at-0x4000:at-0x4000+1+len(text)]=bytes([len(text)])+text
    image[data-0x4000:data-0x4000+256]=bytes(range(256))
    code=bytearray();checkpoints=[];aliaspoints=[];alias_checks={}
    def store(at,value):code.extend(b'\x21'+struct.pack('<H',value)+b'\x22'+struct.pack('<H',at))
    def case(name,op,count=0,source=data,family=None,expected_error=0,expected_actual=None,expected_result=None):
        if family is not None:
            code.extend(b'\x3e'+bytes([family])+b'\x32'+struct.pack('<H',descriptor));store(descriptor+1,2 if family==1 else 128 if family==2 else 0)
        for at,value in [(job,op),(job+6,source),(job+8,count)]:store(at,value)
        code.extend(b'\x21'+struct.pack('<H',job))
        start=0x4080+len(code);code.extend(b'\x00\xcd'+struct.pack('<H',address('dispatch')))
        end=0x4080+len(code);code.append(0)
        checkpoints.append((name,start,end,expected_error,expected_actual,expected_result))
    case('assign_typed',0,source=0x9300,expected_actual=0)
    case('rewrite_typed',2)
    for i in range(8):case('write_typed_'+str(i),5,expected_actual=1)
    case('position',7,expected_result=8);case('size',8,expected_result=8)
    case('seek',6,count=3);case('read_typed',4,expected_actual=1)
    case('eof_false',9,expected_result=0);case('seek_end',6,count=8)
    case('eof_true',9,expected_result=1);case('past_end',4,expected_error=0x99)
    case('flush',10);case('close_typed',3);case('reset_typed',1)
    case('read_after_reset',4,expected_actual=1);case('close_reset',3)
    case('rename',27,source=0x9340);case('erase',11)
    case('assign_block',0,source=0x9380,family=2);case('rewrite_block',2)
    case('write_blocks',14,count=2,expected_actual=2);case('block_seek',6)
    case('read_blocks',12,count=2,expected_actual=2);case('close_block',3)
    case('assign_text',0,source=0x93c0,family=0);case('rewrite_text',2)
    case('write_text',21,source=0x9300);case('newline',23);case('close_text',3)
    case('reset_text',1);case('text_read_char',20,count=3,expected_actual=1)
    case('text_eof',9,expected_result=0);case('text_readln',22);case('text_eof_end',9,expected_result=1)
    case('close_text_input',3);case('missing_reset',1,source=data) # Existing text still opens.
    case('final_close',3)
    if a.serial:
        spec=importlib.util.spec_from_file_location('package',ROOT/'tools/package-native.py')
        pack=importlib.util.module_from_spec(spec);spec.loader.exec_module(pack)
        mock=pack.read_hex(ROOT/'build/assets/sysfile-mock.ihx')
        ms=n.symbols('assets/sysfile-mock')
        assert min(mock)==0x4800 and max(mock)<base
        for at,value in mock.items():image[at-0x4000]=value
        for symbol,method in [('_open','open'),('_close','close'),('_read','read'),('_write','write'),('_ioctl','ioctl')]:
            # Export entries do not expose SDK adapters, so use their labelled
            # fragment IDs from the current assembly's standard link map only
            # for discovering the adapter offsets. The benchmark unit itself
            # still determines all production placement through its metadata.
            own=n.symbols('assets/sysfile')
            unit_payload=len(unit)-len(payload)
            at=base+own[symbol]-unit_payload
            code.extend(b'\x3e\xc3\x32'+struct.pack('<H',at))
            store(at+1,ms['_mock_'+method])
        for at in range(0xf110,0xf11c,2):store(at,0)
        store(0xf100,0x0d51)
        store(0x9300,0x4104);store(0x9302,0x5855);store(0x9304,0x003a)
        image[0x9320-0x4000:0x9324-0x4000]=b'\x03ABC'
        case('aux_assign',0,source=0x9300,family=0)
        case('aux_read',20,count=3,expected_actual=1)
        case('aux_begin',28,count=2)
        case('aux_eoln',24,expected_result=1)
        case('aux_readln',22)
        case('aux_write',21,source=0x9320)
        case('aux_newline',23,source=0x9320)
        store(0xf117,1)
        case('aux_write_failure',21,source=0x9320,expected_error=0xf0)
        store(0xf117,0);store(0xf119,1)
        case('aux_read_failure',20,count=3,expected_error=0x99)
        store(0xf119,0);store(0xf11a,4)
        case('aux_unavailable',21,source=0x9320,expected_error=0x20)
        store(0xf11a,0);store(0xf118,11)
        case('aux_mode_failure',21,source=0x9320,expected_error=0x20)
        # Pure address-validation calls exercise both program-window alias
        # branches without dereferencing the queried addresses or remapping RAM.
        ids=(ROOT/'units/sysfile/sysfile.asm').read_text()
        fid=int(re.search(r'^f_readable_id = (\d+)$',ids,re.M)[1])
        readable=base+records[fid][0]
        store(0x4018,0x579a)
        for mode,limit,at,size,accepted in [(2,0xb000,0xe200,32,1),(2,0xb000,0xefff,2,0),
                                          (4,0x7000,0xa200,32,1),(4,0x7000,0xafff,2,0)]:
            code.extend(b'\x3e'+bytes([mode])+b'\x32\x12\x40')
            store(job+18,limit);store(job+30,0x6800)
            code.extend(b'\x21'+struct.pack('<H',at)+b'\x11'+struct.pack('<H',size))
            start=0x4080+len(code);code.extend(b'\x00\xcd'+struct.pack('<H',readable))
            end=0x4080+len(code);code.append(0)
            aliaspoints.append((f'flags_{mode}_'+('accepted'if accepted else'rejected'),start,end,accepted))
        assert 0x4080+len(code)<0x4800
    finish=0x4080+len(code);code.extend(b'\xc9');assert 0x4080+len(code)<base
    image[0x80:0x80+len(code)]=code;rows={};snapshots={}
    class Probe(n.DebugConsole):
        def command(self,command,capture=False):
            if command!='@measure':return super().command(command,capture)
            call=lambda v:super(Probe,self).command(v)
            def stop(at):
                call(f'bp 0x{at:x}');call('run 2000000000');state=call('regs');call(f'bc 0x{at:x}')
                if f'PC={at:04x} ' not in state:raise RuntimeError('Missed benchmark breakpoint: '+state)
                return int(re.search(r'cyc=(\d+)',call('s'))[1])
            for name,start,end,error,actual,result in checkpoints:
                begin=stop(start);elapsed=stop(end)-begin
                memory=n.memory(call(f'rb 0x{job:x} 38'));raw=bytes(memory[job+i] for i in range(38))
                assert word(raw,22)==error,(name,raw.hex())
                if actual is not None:assert word(raw,10)==actual,(name,raw.hex())
                if result is not None:assert word(raw,20)==result,(name,raw.hex())
                if name=='aux_read':
                    assert n.memory(call('rb 0x9400 1'))[0x9400]==ord('Q')
                if name in ('aux_write_failure','aux_read_failure','aux_unavailable','aux_mode_failure'):
                    assert word(raw,24)==(4 if name=='aux_unavailable' else 1)
                rows[name]=elapsed;snapshots[name]=raw.hex()
            for name,start,end,accepted in aliaspoints:
                stop(start);stop(end)
                af=int(re.search(r'AF=([0-9a-fA-F]{4})',call('regs'))[1],16)
                assert af>>8==accepted,(name,af)
                alias_checks[name]=True
            # The final marker step already stops immediately before RET.
            # Observe owned RAM here; running again would execute the fixture's
            # return and let the kernel unmap the evidence buffers.
            assert f'PC={finish:04x} ' in call('regs')
            if a.serial:
                mock=n.memory(call('rb 0xf110 12'));out=n.memory(call('rb 0xf200 5'))
                assert bytes(out[0xf200+i]for i in range(5))==b'ABC\r\n'
                assert [mock[at]for at in (0xf110,0xf111,0xf112,0xf114,0xf115,0xf116)]==[2,5,0,8,7,12],mock
            return 'SYSFILE measurements complete\n'
    commands=['run 100000000']+n.type_text('H:/BENCH.COM',ticks=100000000)+['tap ENTER','@measure','quit']
    n.execute('sysfile-benchmark',commands,{'BENCH.COM':bytes(image)},timeout=300,console_class=Probe,base_programs=())
    folder=n.RUN/'sysfile-benchmark';files=folder/'hostfs'
    assert not (folder/'input-error.json').exists(),(folder/'input-error.json').read_text()
    assert not (files/'BENCH.DAT').exists() and not (files/'RENAMED.DAT').exists()
    assert (files/'BLOCK.DAT').read_bytes()==bytes(range(256))
    assert (files/'TEXT.DAT').read_bytes()==b'H:/BENCH.DAT\r\n\x1a'
    report={'passed':True,'scope':'Private SYSFILE ABI in unmodified emulator; CPU T-states include CALL and one NOP, interrupts enabled, kernel/disk work included. Dependency sizes exclude the Pascal runtime and caller.',
        'alias_checks':alias_checks,'serial_mock_passed':True if a.serial else None,'unit_sha256':hashlib.sha256(unit).hexdigest(),'unit_bytes':len(unit),'code_bytes':word(unit,32),'workspace_bytes':word(unit,34),
        'serial_scope':'Production AUX logic with a test-only SER0 transport, not physical serial timing' if a.serial else None,
        'cycles':rows,'job_snapshots':snapshots,'linked_sizes':{name:retained(roots) for name,roots in {
            'assign':['dispatch','table'],'typed_read':['dispatch','table','reset','read','close'],
            'typed_write':['dispatch','table','rewrite','write','close'],'block_io':['dispatch','table','reset','rewrite','blocks','close'],
            'text_io':['dispatch','table','reset','rewrite','text_read','text_write','text_readln','close'],
            'rename':['dispatch','table','rename']}.items()},'capture':str(folder)}
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='job_snapshots'},indent=2))
if __name__=='__main__':main()
