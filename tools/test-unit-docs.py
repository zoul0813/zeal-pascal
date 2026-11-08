#!/usr/bin/env python3
"""Check that shipped Unit references cover their public Pascal interfaces."""
import importlib.util,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def clean(text):
    return re.sub(r'\{.*?\}|\(\*.*?\*\)','',text,flags=re.S)
def main():
    interface=(ROOT/'units/zvb/ZVB.PAS').read_text().split('interface',1)[1].split('implementation',1)[0]
    guide=(ROOT/'docs/zvb-library.md').read_text();crt=(ROOT/'docs/crt-library.md').read_text()
    names=re.findall(r'\b(?:function|procedure)\s+(\w+)\b',clean(interface),re.I)
    assert len(names)==45 and len(set(names))==45
    for name in names:assert guide.count(name)>=2,('undocumented ZVB routine',name)
    # Typed declarations and all convenience constants are copied verbatim;
    # the accompanying prose documents semantics, rather than only listing names.
    assert interface[interface.index('{ Graphics:'):].strip() in guide
    assert interface.split('{$I ZVBHW.INC}',1)[1].split('{ Graphics:',1)[0].strip() in guide
    types=re.findall(r'^\s*(\w+)\s*=',interface.split('type',1)[1].split('{$I',1)[0],re.M)
    assert len(types)==17
    for name in types:assert '`'+name+'`'in guide,('undocumented type',name)
    # Native CRT's package interface is authored from this export list.
    spec=importlib.util.spec_from_file_location('screen_package',ROOT/'tools/package-screen.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    crt_names=module.NAMES
    assert len(crt_names)==16
    for name in crt_names:
        assert re.search(r'`(?:function|procedure)\s+'+name+r'\b',crt,re.I),('undocumented CRT routine',name)
    # Check local documentation links, including paths outside docs/.
    links=0
    for path in ('docs/zvb-library.md','docs/zvb-registers.md','docs/crt-library.md'):
        doc=ROOT/path
        for target in re.findall(r'\]\(([^)]+)\)',doc.read_text()):
            if '://'in target or target.startswith('#'):continue
            assert (doc.parent/target.split('#',1)[0]).exists(),(path,target)
            links+=1
    print(json.dumps({'passed':True,'ZVB_routines':len(names),'ZVB_types':len(types),'CRT_routines':len(crt_names),'local_links':links}))
if __name__=='__main__':main()
