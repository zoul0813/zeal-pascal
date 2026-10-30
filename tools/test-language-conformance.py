#!/usr/bin/env python3
"""Replay the preserved language corpus on Zeal, with explicit native corrections.

Requires the local preserved research corpus/evidence; missing inputs are errors.
Original fixtures/binaries are never rewritten. Libraries/overlays are separate.
"""
import argparse
import hashlib
import importlib.util
import json
import re
import struct
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
REFERENCE=ROOT/'tests/cpm/language'
LEXICAL_ORACLES=ROOT/'analysis/language-conformance/lexical-oracles-v2/summary.json'

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def materialize(text):
    return text.replace('\r\n','\n').replace('\r','\n').replace('\n','\r\n').encode('ascii')+b'\x1a'

def runtime_output(raw):
    parts=raw.rsplit(b'Running\r\n',1);assert len(parts)==2
    output,separator,_=parts[1].partition(b'\x1bB1\x1bC1\r\n>');assert separator
    return output

def cases():
    ledger=json.loads((ROOT/'docs/language-evidence.json').read_text())
    evidence={row['case']:row for row in ledger['cases']}
    assert ledger['passed'] and ledger['case_count']==91
    rows=[];deferred=[]
    corrections={'errminlit':(0,'DV-LANG-001'), 'id127':(0,'DV-SOURCE-001'),
        'id128':(100,'DV-SOURCE-001'),'line126':(58,'DV-SOURCE-001'),
        'literal116':(0,'DV-SOURCE-001'),'enumsize':(98,'Native 512-symbol capacity')}
    for name in ['line127','line128','line254','line255','line256','literal117',
                 'literal118','literal124','literal125','literal126','literal255','literal256']:
        corrections[name]=(100,'DV-SOURCE-001')
    def original_output(name,report):
        raw=ROOT/Path(evidence[name]['report']).parent/'session-01.stdout.bin'
        assert sha(raw)==report['sessions'][0]['stdout_sha256']
        return runtime_output(raw.read_bytes())
    for scenario in sorted(REFERENCE.glob('*.json')):
        name=scenario.stem;entry=evidence[name]
        assert sha(scenario)==entry['scenario_sha256']
        report_path=ROOT/entry['report'];assert sha(report_path)==entry['report_sha256']
        report=json.loads(report_path.read_text());assert report['passed'] and report['originals_unchanged']
        spec=json.loads(scenario.read_text())
        # Match run-cpm.py's recorded disk bytes, including CRLF and EOF. This
        # also matters when a Pascal program reads an INC fixture as raw data.
        files={k:materialize((scenario.parent/v).read_text(encoding='ascii'))
               for k,v in spec['files'].items()}
        assert all(hashlib.sha256(data).hexdigest()==report['input_files'][k]['sha256']for k,data in files.items()),name
        root=next(k for k in files if k.endswith('.PAS'))
        if name in ('userinterrupt','userraw'):
            deferred.append({'case':name,'reason':'Physical Ctrl-C delivery; run current native keyboard/interrupt gates separately'})
            continue
        errors=[int(m[1])for session in spec['sessions']for step in session['steps']
                if (m:=re.fullmatch(r'Error (\d+)\.',step.get('expect','')))]
        code=errors[0]if errors else 56 if name=='include-repair'else 0
        disposition='Same source and numeric diagnostic/output as the preserved reference'
        if name in corrections:code,disposition=corrections[name]
        output=b''
        if not code:
            if name=='errminlit':output=b''
            elif name=='id127':output=b'IDENTIFIER-LENGTH-OK\r\n'
            elif name=='literal116':output=b'LITERAL-LENGTH-OK\r\n'
            else:output=original_output(name,report)
            if name=='emptyrecord':
                output=re.sub(rb'^FILE=\d+\r\n',b'FILE=16\r\n',output,flags=re.M)
                disposition='Native16-byte file control block; explicitly excluded from ordinary data-layout equivalence'
        rows.append({'id':name,'root':root,'files':files,'diagnostic':code,'output':output,'disposition':disposition})
        if name=='include-repair':
            fixed=dict(files);repaired=report_path.parent/'disk/BROKEN.INC'
            assert sha(repaired)==report['output_files']['BROKEN.INC']['sha256']
            fixed['BROKEN.INC']=repaired.read_bytes()
            rows.append({'id':'include-repaired','root':root,'files':fixed,'diagnostic':0,
                'output':original_output(name,report),'disposition':'Exact recorded repaired include bytes and runtime output'})
        if name=='enumsize':
            text=files[root].decode()
            for width in (255,256,257):
                declaration=re.search(r't'+str(width)+r'=\(.*?\);',text,re.S)[0]
                source=f'program E;type {declaration}\nbegin writeln(\'ENUM{width}=\',sizeof(t{width}))end.\n'.encode()
                rows.append({'id':f'enum-{width}','root':'MAIN.PAS','files':{'MAIN.PAS':source},'diagnostic':0,
                    'output':f'ENUM{width}={1 if width<=256 else 2}\r\n'.encode(),
                    'disposition':'Same enum definition/size oracle, isolated to respect native symbol capacity'})
    for name in ('source-lines','integer-limits'):
        for vector in json.loads((REFERENCE/'native'/f'{name}.json').read_text())['vectors']:
            expected=vector['expected'];files=vector.get('files',{'MAIN.PAS':vector.get('source','')})
            rows.append({'id':vector['id'],'root':'MAIN.PAS','files':{k:v.encode()for k,v in files.items()},
                'diagnostic':expected['diagnostic']or 0,
                'output':b''.join(line.encode()+b'\r\n'for line in expected['runtime_output']),
                'disposition':'Explicit corrected-native acceptance vector'})
    lexical=json.loads(LEXICAL_ORACLES.read_text());assert lexical['passed']
    for case in lexical['cases']:
        path=ROOT/case['report'];assert sha(path)==case['report_sha256']
        report=json.loads(path.read_text());assert report['passed'] and report['originals_unchanged']
        stdout=path.parent/'session-01.stdout.bin';assert sha(stdout)==report['sessions'][0]['stdout_sha256']==case['stdout_sha256']
        source=materialize(case['source']);assert hashlib.sha256(source).hexdigest()==report['input_files']['PROBE.PAS']['sha256']
        raw=stdout.read_bytes();codes=re.findall(rb'Error (\d+)\.',raw)
        code=int(codes[0])if codes else 0;assert code==case['diagnostic']
        output=b''if code else runtime_output(raw);assert output==bytes.fromhex(case['output_hex'])
        rows.append({'id':case['id'],'root':'MAIN.PAS','files':{'MAIN.PAS':source},
            'diagnostic':code,'output':output,
            'disposition':'Additional observed original lexer boundary; source and output bound to retained oracle',
            'oracle_paths':[path,path.parent/'session-01.stdout.bin']})
    for switch in 'gpdfk':
        rows.append({'id':'native-directive-'+switch,'root':'MAIN.PAS',
            'files':{'MAIN.PAS':('begin {$'+switch+'+}end.').encode()},'diagnostic':93,'output':b'',
            'disposition':'Native contract rejects unsupported16-bit directives; not an observed original diagnostic claim'})
    pins=json.loads((ROOT/'tests/native/language-diagnostics.json').read_text())['cases']
    assert {r['id']for r in rows if r['diagnostic']}==set(pins)
    for row in rows:
        if row['diagnostic']:
            row['error']=pins[row['id']];assert row['diagnostic']==row['error']['code']
    return rows,deferred

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',action='append',help='Run only a named case (repeatable)')
    parser.add_argument('--prepare-only',action='store_true',help='Inspect corpus classification without running Zeal')
    args=parser.parse_args();rows,deferred=cases()
    if args.case:
        chosen=set(args.case);rows=[r for r in rows if r['id']in chosen]
        assert {r['id']for r in rows}==chosen,'Unknown case'
    assert 0<len(rows)<=200
    if args.prepare_only:
        print(json.dumps({'cases':len(rows),'compile_failures':sum(bool(r['diagnostic'])for r in rows),
            'executions_in_each_mode':sum(not r['diagnostic']for r in rows),'separate_physical_cases':deferred},indent=2));return
    module=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
    n=importlib.util.module_from_spec(module);module.loader.exec_module(n)
    target='zcorpus';sym=n.symbols(target);checkpoint=sym['_corpus_checkpoint']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    manifest=bytearray(struct.pack('<H',len(rows)))
    for i,row in enumerate(rows):
        folder=f'C{i:03d}';row['folder']=folder;path=f'H:/{folder}/{row["root"]}'.encode()
        assert len(path)<128
        manifest+=bytes([row['diagnostic']])+path+bytes(128-len(path))
        fixtures.update({folder+'/'+k:v for k,v in row['files'].items()})
    fixtures['CORPUS.DAT']=bytes(manifest)
    paths=[Path(__file__).resolve(),ROOT/'CMakeLists.txt',ROOT/'tools/test-native.py',n.EMULATOR,n.ROM,n.KERNEL_MAP]
    paths+=list((ROOT/'src').glob('*'))+list((ROOT/'tests/native').glob('*'))
    paths+=list(REFERENCE.rglob('*'))+[ROOT/'docs/language-evidence.json']
    paths += [LEXICAL_ORACLES,ROOT/'docs/language-specification.md',ROOT/'docs/language-coverage.json']
    for row in rows:paths+=row.get('oracle_paths',[])
    paths+=[ROOT/f'build/{target}.{ext}'for ext in ('bin','zpl','map','banks.json')]
    hashes={str(p):sha(p)for p in paths if p.is_file()}
    commands=['run 100000000']+n.program_launch(target)
    for row in rows:
        commands += [n.ImageStop(checkpoint,target+'.bin'),'mmu','rb 0x4000 32',
            f"rb 0x{sym['_corpus_data_page']:x} 1",f"rb 0x{sym['_corpus_result']:x} 8",
            f"rb 0x{sym['_corpus_error_path']:x} 128",
            f"rb 0x{sym['_test_failed']:x} 2",f"rb 0x{sym['_test_passed']:x} 2",'s']
    commands += [f"bp 0x{sym['_test_stop']:x}",'run 1000000000','regs','mmu','rb 0x4000 32',
        f"rb 0x{sym['_corpus_data_page']:x} 1",f"rb 0x{sym['_test_done']:x} 1",
        f"rb 0x{sym['_test_failed']:x} 2",f"rb 0x{sym['_test_passed']:x} 2",f"rb 0x{sym['_test_stage']:x} 2",'quit']
    raw=n.execute('language-conformance',commands,fixtures,timeout=1800)
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    expected_prefix=(ROOT/f'build/{target}.bin').read_bytes()[:32]
    def qualified(frame,pc):
        data=n.memory(frame);page=data.get(sym['_corpus_data_page'],0)
        mappings=re.findall(r'\[DBG\] c000 -> ([0-9a-fA-F]+)',frame)
        good=frame.startswith(f'{pc:04x} ')and bytes(data.get(0x4000+i,0)for i in range(32))==expected_prefix
        good=good and 32<=page<64 and bool(mappings)and int(mappings[-1],16)==page*16384
        return data,good
    observed=[qualified(f,checkpoint)for f in frames if f.startswith(f'{checkpoint:04x} ')]
    disk=n.RUN/'language-conformance/hostfs';results=[];prior_failed=0
    for i,row in enumerate(rows):
        checks={};actual=b'';diagnostic=None
        if i<len(observed):
            memory,supervisor=observed[i];at=sym['_corpus_result'];data=bytes(memory.get(at+j,0)for j in range(8))
            failures=memory.get(sym['_test_failed'],0)|(memory.get(sym['_test_failed']+1,0)<<8)
            checks={'authenticated supervisor code and data pages':supervisor,
                'exact case checkpoint':int.from_bytes(data[:2],'little')==i,
                'expected compile diagnostic':data[4]==row['diagnostic'],
                'compile acceptance':bool(data[3])==(row['diagnostic']==0),
                'native RAM/saved execution and ownership checks':failures==prior_failed}
            prior_failed=failures
            if row['diagnostic']:
                error_path=bytes(memory.get(sym['_corpus_error_path']+j,0)for j in range(128)).split(b'\0')[0].decode('ascii',errors='replace')
                diagnostic={'code':data[4],'start':int.from_bytes(data[5:7],'little'),'path':error_path}
                checks['exact native diagnostic start']=diagnostic['start']==row['error']['start']
                checks['exact diagnostic source identity']=error_path==f'H:/{row["folder"]}/{row["error"]["file"]}'
            if not row['diagnostic']:
                out=disk/row['folder']/'OUTPUT.TXT';actual=out.read_bytes()if out.is_file()else b''
                checks['runtime output file exists']=out.is_file()
                expected=row['output']*2
                if row['id']=='constaddr':
                    image=(disk/row['folder']/Path(row['root']).with_suffix('.COM')).read_bytes()
                    start,size,base,globals_=struct.unpack_from('<HHHH',image,20)
                    constants=[int(v)&65535 for v in re.findall(rb'CONST-ADDR=(-?\d+)',actual)]
                    variables=[int(v)&65535 for v in re.findall(rb'VAR-ADDR=(-?\d+)',actual)]
                    checks['native typed/global addresses occupy their proper ranges']=len(constants)==len(variables)==2 and all(start<=v<start+size for v in constants)and all(base<=v<base+globals_ for v in variables)
                    actual=re.sub(rb'((?:CONST|VAR)-ADDR)=-?\d+',rb'\1=<native>',actual)
                    expected=re.sub(rb'((?:CONST|VAR)-ADDR)=-?\d+',rb'\1=<native>',expected)
                checks['exact reference/native-contract output in both modes']=actual==expected
        else:checks['case completed']=False
        results.append({'id':row['id'],'passed':all(checks.values()),'diagnostic':row['diagnostic'],
            'actual_diagnostic':diagnostic,'disposition':row['disposition'],'checks':checks,'actual_output_hex':actual.hex()})
        print(('PASS 'if all(checks.values())else'FAIL ')+row['id'],flush=True)
    final,supervisor=qualified(frames[-1],sym['_test_stop'])if frames else({},False)
    word=lambda name:final.get(sym[name],0)|(final.get(sym[name]+1,0)<<8)
    completed=supervisor and final.get(sym['_test_done'])==255 and word('_test_stage')==6 and word('_test_failed')==0
    unchanged=all(sha(Path(p))==h for p,h in hashes.items())
    retained=all((disk/p).is_file()and(disk/p).read_bytes()==v for p,v in fixtures.items())
    report={'passed':completed and unchanged and retained and len(observed)==len(rows)and all(r['passed']for r in results),
        'scope':__doc__,'case_count':len(rows),'completed':completed,'all_inputs_unchanged':unchanged,
        'all_fixture_files_retained':retained,'cases':results,'separate_physical_cases':deferred,
        'supervisor_observed':supervisor,'native_assertions_passed':word('_test_passed')if supervisor else None,
        'native_assertions_failed':word('_test_failed')if supervisor else None,
        'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes,
        'full_language_conformance':False,'note':'Physical directive/interrupt and complete requirement audit remain separate gates.'}
    (n.RUN/'language-conformance-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(n.RUN/'language-conformance-report.json',flush=True)
    assert report['passed']
if __name__=='__main__':main()
