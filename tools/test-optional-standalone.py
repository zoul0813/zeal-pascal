#!/usr/bin/env python3
"""Execute optional-runtime COMs in fresh sessions without compiler or library files."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('optional_cases',ROOT/'tools/test-optional-real.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);n=m.n

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('report',type=Path);args=parser.parse_args()
    previous=json.loads(args.report.read_text());assert previous['passed']
    target=previous['optional'].get('target','zoptionalpascal')
    source=ROOT/previous['optional']['capture_directory']/('optional-real-'+target)/'hostfs'
    parent='zexparent';symbols=n.symbols(parent);checks={};runs={}
    cases=[label.removesuffix('-memory')for label,_ in previous['optional']['labels']if label.endswith('-memory')]
    assert cases and set(cases)<=set(m.CASES)
    for name in cases:
        code=m.CASES[name][1]
        image=(source/(name+'.COM')).read_bytes()
        marker=re.search(rb"writeln\('([^']+)'\)",code,re.I).group(1)
        commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
        commands += [f"bp 0x{symbols['_execute_parent_stop']:x}",'tap ENTER']
        if name in m.INPUTS:
            commands += [n.PhysicalLine(m.INPUTS[name],name+'.COM',0)]
        commands += ['run 10000000000','regs','mmu']
        for symbol in ('_test_done','_test_passed','_test_failed'):
            commands += [f'rb 0x{symbols[symbol]:x} 2']
        commands += ['quit']
        fixtures={name+'.COM':image,'INTDATA.TXT':b'42\r\n','REALDATA.TXT':b'1.5\r\n',
                  parent+'.bin':(ROOT/f'build/{parent}.bin').read_bytes(),
                  'PARENT.TXT':bytes([0,0])+('H:/'+name+'.COM').encode()}
        deleted=[]
        def remove_installation(command,response,directory):
            if command=='run 100000000'and not deleted:
                for path in (directory/'hostfs').iterdir():
                    if path.name!=parent+'.bin'and path.suffix.lower()in('.bin','.zpl','.zpu','.pas'):
                        path.unlink();deleted.append(path.name)
                assert deleted
        raw=n.execute('standalone-'+name,commands,fixtures,timeout=180,after_command=remove_installation)
        frames=re.split(r'\[DBG\] PC=',raw)[1:]
        checks[name+' authenticated supervisor completion']=len(frames)==1 and frames[0].startswith(f"{symbols['_execute_parent_stop']:04x} ")
        state=n.memory(raw)
        word=lambda symbol:state.get(symbols[symbol],0)|(state.get(symbols[symbol]+1,0)<<8)
        checks[name+' successful exit and complete page recovery']=state.get(symbols['_test_done'])==255 and word('_test_passed')>0 and word('_test_failed')==0
        folder=n.RUN/('standalone-'+name)/'hostfs'
        output=(folder/'RESULT.TXT').read_bytes()if(folder/'RESULT.TXT').exists()else b''
        checks[name+' captured output']=marker+b'\r\n'in output
        checks[name+' no compiler source or libraries']=not any(path.name!=parent+'.bin'and path.suffix.lower()in('.bin','.zpl','.zpu','.pas')for path in folder.iterdir())
        checks[name+' executable unchanged']=(folder/(name+'.COM')).read_bytes()==image
        runs[name]={'removed':deleted,'image_sha256':m.sha(folder/(name+'.COM')),'stdout_hex':output.hex()}
    report={'passed':bool(checks)and all(checks.values()),'scope':__doc__,'source_report':str(args.report),'checks':checks,'runs':runs,'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'optional-standalone-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
