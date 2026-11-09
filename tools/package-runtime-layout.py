#!/usr/bin/env python3
"""Derive compact runtime placement and operand fixups from qualified objects."""
from pathlib import Path
from collections import Counter
import importlib.util,json,os,re,shlex,struct,sys
from runtime_objects import read_object,read_archive,library_closure,area_symbols,resolve_objects
spec=importlib.util.spec_from_file_location('native_package',Path(__file__).with_name('package-native.py'))
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)

def build(directory):
    directory=directory.resolve();target='integer_runtime'
    sdk=Path(os.environ['ZOS_PATH'])/'sdk/sdcc/lib/z80.lib' if 'ZOS_PATH'in os.environ else Path(os.environ.get('ZP_ZEAL_ROOT',str(Path(__file__).resolve().parents[2])))/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/lib/z80.lib'
    tokens=shlex.split((directory/f'CMakeFiles/{target}.dir/link.txt').read_text())
    objects=[read_object(directory/t)for t in tokens if t.endswith('.rel')]
    members=read_archive(sdk);closure=library_closure(objects,members)
    mapping=(directory/f'{target}.map').read_text()
    names=re.findall(r'^\s+\[ ([^\]\n]+\.rel) \]',mapping.split('Libraries Linked',1)[1],re.M)
    assert Counter(names)==Counter(members[i].name for i in closure)
    for name in names:
        selected=[members[i].object for i in closure if members[i].name==name];assert len(selected)==1
        objects+=selected
    symbols=p.read_symbols(directory/f'{target}.ihx');offsets={};bases={}
    for i,obj in enumerate(objects):
        for j,area in enumerate(obj.areas):
            if not area.absolute:
                bases[i,j]=symbols['s_'+area.name]+offsets.get(area.name,0)
                offsets[area.name]=offsets.get(area.name,0)+area.size
    explicit={tokens[i+1].split('=')[0]:int(tokens[i+1].split('=')[1],0)for i,t in enumerate(tokens)if t=='-b'}
    image,definitions=resolve_objects(objects,bases,area_symbols(objects,bases,explicit))
    assert image==p.read_hex(directory/f'{target}.ihx')
    first=symbols['s__REAL']+symbols['l__REAL'];last=symbols['s__CODE'];gap=last-first
    end=max((a.address if a.absolute else bases[i,j])+a.size for i,o in enumerate(objects)for j,a in enumerate(o.areas)if a.size)
    assert 0x4000<first<last<end<=0x8000
    assert all(not a.size or (a.address if a.absolute else bases[i,j])+a.size<=first or (a.address if a.absolute else bases[i,j])>=last for i,o in enumerate(objects)for j,a in enumerate(o.areas))
    full=p.read_symbols(directory/'pascal_runtime.ihx')
    full_end=max(full[k]+full['l_'+k[2:]] for k in full if k.startswith('s__') and full.get('l_'+k[2:],0))
    assert 0x4000<full_end<=0x8000
    refs=[]
    for i,obj in enumerate(objects):
        for j,area in enumerate(obj.areas):
            base=area.address if area.absolute else bases[i,j]
            for r in area.relocations:
                target=(definitions[obj.symbols[r.target].name]if r.symbol else 0 if obj.areas[r.target].absolute else bases[i,r.target])+r.addend
                if last<=target<end:
                    refs.append((base+r.site,target,('word16','low8','high8').index(r.kind)))
    refs.sort();assert len({site for site,_,_ in refs})==len(refs)
    # Only these immutable entry prefixes are copied by callback adapters.
    # Classify by the object's operand site, never by an immediate's value.
    entries={image[0x40ad]|(image[0x40ae]<<8),symbols['input_open'],symbols['input_echo']}
    copies=[]
    for entry in sorted(entries):
        selected=[r for r in refs if entry<=r[0]<entry+3]
        assert not selected or (len(selected)==1 and selected[0][0]==entry+1 and selected[0][2]==0)
        copies.append((entry+1,selected[0][1] if selected else 0))
    # Word operands may be rewritten by generated adapters only after this pass.
    packed=bytearray(image.get(at,0)for at in range(0x4000,end)if not first<=at<last)
    move=lambda at:at-gap if last<=at<end else at
    for site,target,kind in refs:
        at=move(site)-0x4000;value=move(target)
        if kind==0:packed[at:at+2]=struct.pack('<H',value)
        else:packed[at]=(value>>8 if kind==2 else value)&255
    header=f'''/* Generated from the pinned toolchain's relocatable objects. */
#define RUNTIME_GAP_FIRST {first}u
#define RUNTIME_GAP_LAST {last}u
#define RUNTIME_GAP_BYTES {gap}u
#define RUNTIME_OWNED_END {end}u
#define RUNTIME_FULL_END {full_end}u
#define RUNTIME_COMPACT_END {end-gap}u
#ifdef ZP_RUNTIME_RELOCATIONS
static const struct {{uint16_t site,target;uint8_t kind;}} runtime_relocations[]={{
'''+''.join(f' {{{site}u,{target}u,{kind}}},\n'for site,target,kind in refs)+'};\n#endif\n#ifdef ZP_RUNTIME_COPY_OPERANDS\nstatic const struct {uint16_t site,target;} runtime_copy_operands[]={\n'+''.join(f' {{{site}u,{target}u}},\n' for site,target in copies)+'};\n#endif\n'
    (directory/'runtime-layout.h').write_text(header)
    assets=directory/'assets';assets.mkdir(exist_ok=True)
    (assets/'linked_integer_runtime.bin').write_bytes(packed)
    (directory/'linked_integer_runtime.bin').unlink(missing_ok=True)
    (directory/'runtime-layout.json').write_text(json.dumps({'legacy_end':end,'gap_first':first,'gap_last':last,'recovered_bytes':gap+0x8000-end,'compact_end':end-gap,'runtime_bytes':len(packed),'references':len(refs)},indent=2)+'\n')
    print((directory/'runtime-layout.json').read_text())
if __name__=='__main__':build(Path(sys.argv[1]))
