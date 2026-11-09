#!/usr/bin/env python3
"""Check that shipped Unit references cover their public Pascal interfaces."""
import importlib.util,json,re,subprocess
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
    # Compare the explicit public indexes, not incidental prose mentions.
    runtime=(ROOT/'docs/runtime.md').read_text()
    language=(ROOT/'docs/language.md').read_text()
    frontend=(ROOT/'src/compiler/frontend.c').read_text()
    expressions=(ROOT/'src/compiler/expressions.c').read_text()
    def strings(text, marker):
        values=re.findall(r'"([a-z][a-z0-9_]*)"',
                          text.split(marker,1)[1].split('};',1)[0])
        assert values,('empty source inventory',marker)
        return set(values)
    keywords=strings(frontend,'reserved[] = {')
    assert len(keywords)==48
    reserved=re.search(r'The reserved words are:.*?```text\n(.*?)```',language,re.S)
    assert reserved and set(reserved[1].split())==keywords,'reserved-word inventory differs'
    intrinsics=strings(expressions,'functions[]={')
    # These two entries exist only in the legacy non-Unit compiler.
    assert {'wherex','wherey'}<=intrinsics
    intrinsics-={'wherex','wherey'}
    assert len(intrinsics)==35
    public=set(intrinsics)
    # All grammar owners that dispatch predefined names. Other builtin files
    # emit code for these parsed names; CRT's production names come from its Unit.
    for source in ('src/compiler/expressions.c','src/compiler/statements.c',
                   'src/compiler/codegen/builtins/files.c',
                   'src/compiler/codegen/builtins/syscalls.c'):
        public |= set(re.findall(r'cc_is\("([a-z][a-z0-9_]*)"\)',
                                 (ROOT/source).read_text()))-keywords
    for table,count in (('hook_names',5),('standard_names',8),('system_variables',4)):
        entries=strings(frontend,table+'[]={')
        assert len(entries)==count,('source table changed',table)
        public |= entries
    assert len(public)==99
    index=runtime.split('## Predefined name index',1)[1].split(
        '## Numeric and ordinal routines',1)[0]
    indexed=set(re.findall(r'`([A-Za-z][A-Za-z0-9_]*)`',index.lower()))
    assert indexed==public,('predefined inventory differs',sorted(public-indexed),sorted(indexed-public))
    colors=strings(frontend.split('int16_t cc_predefined_color',1)[1],'names[]={')
    assert len(colors)==16
    assert colors<=set(re.findall(r'`([a-z]+)`',crt.lower())),'missing color constant'
    messages=(ROOT/'src/compiler/messages.c').read_text()
    expected={int(code):text for code,text in re.findall(
        r'^\s*\[(\d+)\]="([^"]*)"',messages,re.M)}
    assert len(expected)==78,'compiler message extraction changed'
    unit_messages=re.findall(r'"([^"]*)"',messages.split(
        'unit_messages[]={',1)[1].split('};',1)[0])
    assert len(unit_messages)==6
    expected.update(enumerate(unit_messages,248))
    cycle=re.search(r'code==247.*?output\(": ([^"]+)"\)',messages)
    assert cycle
    expected[247]=cycle[1]
    catalog=(ROOT/'docs/errors.md').read_text().split(
        '## Compiler message catalog',1)[1].split('## Common compiler errors',1)[0]
    rows=re.findall(r'^\| (\d+) \| ([^|]+) \|$',catalog,re.M)
    documented={int(code):text for code,text in rows}
    assert len(rows)==len(documented)==85
    assert documented==expected,'compiler catalog differs'
    # Check local documentation links, including paths outside docs/.
    # Ignore local research when validating what readers receive from Git.
    published = {ROOT/path for path in subprocess.check_output(
        ['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'],
        cwd=ROOT).decode().split('\0') if path}
    links=0
    documents=sorted(p for p in published if p.suffix=='.md' and p.is_file())
    for doc in documents:
        path = str(doc.relative_to(ROOT))
        for target in re.findall(r'\]\(([^)]+)\)',doc.read_text()):
            if '://'in target or target.startswith('mailto:'):continue
            filename, _, anchor = target.partition('#')
            resolved = (doc.parent/filename).resolve() if filename else doc
            if anchor and resolved.is_file() and resolved.suffix == '.md':
                headings = re.findall(r'^#{1,6} +(.+)',resolved.read_text(),re.M)
                anchors = {re.sub(r'[^\w\- ]','',h.lower()).replace(' ','-')
                           for h in headings}
                assert anchor in anchors,('missing section',path,target)
            assert ((resolved.is_file() and resolved in published) or
                    (resolved.is_dir() and any(p.is_relative_to(resolved)
                                              for p in published))), (path,target)
            links+=1
    print(json.dumps({'passed':True,'ZVB_routines':len(names),'ZVB_types':len(types),'CRT_routines':len(crt_names),'predefined_names':len(public),'intrinsic_functions':len(intrinsics),'reserved_words':len(keywords),'compiler_messages':len(expected),'colors':len(colors),'markdown_files':len(documents),'local_links':links}))
if __name__=='__main__':main()
