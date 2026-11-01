#!/usr/bin/env python3
"""Native completed-member spooling and nested group ownership; not Pascal overlays."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    target = 'zmemberstest'
    sym = n.symbols(target)
    # Bank14 begins the parkable compiler phases; it is not the bank count.
    assert sym['_compiler_compile_project'] >> 16 == 14
    assert sym['_code_phase_restore'] >> 16 < 14
    phase_first = (sym['_compiler_compile_project'] >> 16)-1
    bank_count = n.code_banks(target)
    phase_count = bank_count-phase_first
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes() for ext in ('bin', 'zpl')}
    library = fixtures[target+'.zpl']
    assert phase_first < bank_count <= 16
    assert library[:6] == b'ZPL1\x02'+bytes([bank_count])
    header_bytes = 16+2*bank_count
    assert int.from_bytes(library[6:8], 'little') == header_bytes
    sizes = struct.unpack_from(f'<{bank_count}H',library,16)
    assert all(0 < size <= 16384 for size in sizes)
    assert len(library) == header_bytes+sum(sizes)
    assert fixtures[target+'.bin'][sym['_code_required']-0x4000] == bank_count
    inputs = [Path(__file__), ROOT/'CMakeLists.txt', ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM]
    inputs += sorted((ROOT/'src').glob('*'))
    inputs += [ROOT/'tests/native'/name for name in ('members-main.c', 'members-probe.asm', 'late-cases.h', 'harness.c', 'harness.h', 'machine.asm')]
    inputs += [ROOT/f'build/{target}.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    commands = ['run 100000000'] + n.program_launch(target)
    commands += [f"bp 0x{sym['_test_stop']:x}", 'run 60000000000', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    raw = n.execute('member-spool', commands, fixtures, timeout=600)
    memory = n.memory(raw)
    word = lambda name: memory.get(sym[name], 0) | memory.get(sym[name]+1, 0) << 8
    spool = n.RUN/'member-spool/hostfs/MEMBERS.SPL'
    data = spool.read_bytes() if spool.exists() else b''
    # Independent serialization oracle. Keep no product emitter/hash code in
    # the expected section framing, offset calculation or byte patterns.
    header = b'ZPMS\2\0' + struct.pack('<6H', 24, 0xbff1, 528, 15, 3, 4) + bytes(6)
    code = bytes(((at-0x8000)*3+9 if at < 0xc000 else (at-0xc000)*5+1) & 255
                 for at in range(0xbff1, 0xc201))
    maps = bytes((at*7+3) & 255 for at in range(199, 247))
    relocs = bytes((at*11+5) & 255 for at in range(12, 28))
    expected = header + code + maps + relocs
    def frame(origin, entry, code, first_map, maps, first_reloc, relocs):
        return (b'ZPMS\2\0'+struct.pack('<6H', 24, origin, len(code), entry-origin, maps, relocs)+bytes(6)
                +code+bytes((at*7+3)&255 for at in range(96+71+first_map*16, 96+71+(first_map+maps)*16))
                +bytes((at*11+5)&255 for at in range(first_reloc*4, (first_reloc+relocs)*4)))
    nested_first = frame(0xbff8, 0xbffc, bytes(range(0x30, 0x30+20)), 3, 2, 4, 2)
    nested_second = frame(0xbff8, 0xc000, bytes(range(0x40, 0x40+40)), 3, 1, 4, 1)
    parent = frame(0xbff0, 0xc020, bytes(range(0x10, 0x18))+bytes(40)+bytes(range(0x60, 0x68)), 2, 2, 3, 2)
    sibling = frame(0xbff0, 0xbff0, bytes(range(0x70, 0x70+24)), 2, 1, 3, 1)
    grouped = n.RUN/'member-spool/hostfs/GROUPS.SPL'
    group_data = grouped.read_bytes() if grouped.exists() else b''
    late_first = (b'ZPMS\2\0'+struct.pack('<7H',24,0xbffa,10,0,0,2,2)+bytes(4)
                  +b'\xcd\0\0'+bytes(4)+b'\xcd\0\0'
                  +struct.pack('<4H',0x3ffb,1,0x4002,1)
                  +struct.pack('<4H',0xc002,5,1,0)+struct.pack('<4H',0xbffb,5,1,0))
    late_second = (b'ZPMS\2\0'+struct.pack('<7H',24,0xbffa,3,0,0,1,1)+bytes(4)
                   +b'\x21\x03\0'+struct.pack('<2H',0x3ffb,1)+struct.pack('<4H',0xbffb,2,1,0))
    late_spool=n.RUN/'member-spool/hostfs/LATE.SPL'
    def records_from(name):
        path=n.RUN/('member-spool/hostfs/'+name)
        if not path.is_file():return []
        payload=path.read_bytes();records=[];offset=0
        while offset+24<=len(payload):
            h=payload[offset:offset+24]
            header_bytes,origin,code_bytes,entry,maps,relocs,lates=struct.unpack_from('<7H',h,6)
            end=offset+24+code_bytes+maps*16+relocs*4+lates*8
            if (h[:6]!=b'ZPMS\2\0' or header_bytes!=24 or maps or end>len(payload)
                    or not code_bytes or entry>=code_bytes or h[20:24]!=bytes(4)):return []
            code=payload[offset+24:offset+24+code_bytes]
            journal=payload[end-lates*8:end] if lates else b''
            terms=[struct.unpack_from('<4H',journal,i)for i in range(0,len(journal),8)]
            if not all(coefficient==1 and flags==0 and origin<=site<origin+len(code)-1
                       for site,key,coefficient,flags in terms):return []
            records.append((origin,code,terms,payload[offset:end]));offset=end
        return records if offset==len(payload) else []
    helpers=records_from('HELPERS.SPL')
    helper_ok=False
    if len(helpers)==2:
        origin,code,terms,_=helpers[0]
        helper_ok=([key for site,key,coefficient,flags in terms]==[0,1,2,2]
                   and [int.from_bytes(code[site-origin:site-origin+2],'little')for site,key,coefficient,flags in terms]==[0,3,0,0]
                   and not helpers[1][2] and helpers[1][0]>=origin+len(code))
    owners=records_from('OWNERS.SPL');owner_ok=False
    if len(owners)==2:
        origin,code,terms,frame=owners[0]
        owner_ok=([key for site,key,coefficient,flags in terms]==[1,2,3,4,6,7,8]
                  and all(code[site-origin:site-origin+2]==b'\0\0'for site,key,coefficient,flags in terms)
                  and not owners[1][2] and owners[1][0]>=origin+len(code)
                  )
    checks = {
        'native completion': memory.get(sym['_test_done']) == 255 and word('_test_stage') == 6,
        'native group/journal ownership, scalar/byte/multiple-symbol fixups, phase parking, compile, RAM/saved Run and MMU assertions': word('_test_failed') == 0 and word('_test_passed') >= 147,
        'exact independently framed two-member spool': data == expected*2,
        'nested member extents, zero parent gaps and immutable suffixes match independent disk framing': group_data == nested_first+nested_second+parent+sibling,
        'host compares restored compiler bytes to library payload':all((n.RUN/'member-spool/hostfs'/name).is_file()and(n.RUN/'member-spool/hostfs'/name).read_bytes()==library[header_bytes:]for name in ('CODE0.BIN','CODE1.BIN')),
        'member late chains and bridge references match independent byte framing': late_spool.is_file() and late_spool.read_bytes()==late_first+late_second,
        'production parameter/random factories defer to root and later members reuse them': helper_ok,
        'keyboard/interrupt/file/record-array-file initializers and all root chains bind outside member reservations': owner_ok,
    }
    marker=sym['_test_phase_restore_marker']
    assert 0x4000<=marker<0x8000
    def probes():
        result=['regs','mmu']
        for name,size in (('_test_done',1),('_test_passed',2),('_test_failed',2),('_test_stage',2),
                          ('_code_owned',1),('_code_phase_fd',1),('_code_pages',bank_count),('_code_phase_pending',phase_count),
                          ('_test_failures',128)):
            result += [f'rb 0x{sym[name]:x} {size}']
        return result
    altered=[]
    offset=header_bytes+sum(sizes[:phase_first])
    def alter_on_marker(command,response,directory):
        if command!='regs' or f'PC={marker:04x} ' not in response:return
        path=directory/'hostfs'/f'{target}.zpl'
        assert len(altered)<2
        if altered:
            assert path.read_bytes()==library[:offset]
            path.write_bytes(library)
        else:
            assert path.read_bytes()==library
            with path.open('r+b') as stream:stream.truncate(offset)
        altered.append({'operation':'repair' if altered else 'corrupt','offset':offset,'sha256':n.sha(path)})
    negative=['run 100000000']+n.program_launch(target)+[f'bp 0x{marker:x}',f"bp 0x{sym['_test_stop']:x}",
              'run 60000000000']+probes()+['s','run 60000000000']+probes()
    negative += [f'bc 0x{marker:x}','s','run 60000000000']+probes()+['quit']
    negative_fixtures={**fixtures,'PHASE.BAD':b'1'}
    negative_raw=n.execute('member-phase-corrupt',negative,negative_fixtures,timeout=600,after_command=alter_on_marker)
    negative_state=n.memory(negative_raw)
    value=lambda name:negative_state.get(sym[name],0)|(negative_state.get(sym[name]+1,0)<<8)
    checks['real post-startup library truncation rejects compilation, preserves source/build, and repairs on retry']=(
        len(altered)==2 and negative_state.get(sym['_test_done'])==255 and value('_test_failed')==0 and value('_test_passed')>=150
        and all((n.RUN/'member-phase-corrupt/hostfs'/name).is_file()and(n.RUN/'member-phase-corrupt/hostfs'/name).read_bytes()==library[header_bytes:]for name in ('CODE0.BIN','CODE1.BIN'))
        and (n.RUN/f'member-phase-corrupt/hostfs/{target}.zpl').read_bytes()==library)
    release,exit_pc,close_pc=sym['_code_release'],sym['_exit'],sym['_close']
    kernel=n.kernel_symbols();owners=kernel['_page_owners'];kernel_exit=kernel['zos_loader_exit']
    natural=['run 100000000']+n.program_launch(target)+[f'bp 0x{release:x}',f'bp 0x{exit_pc:x}',
             'run 60000000000']+probes()+[f'rb 0x{owners:x} 32',f'bc 0x{release:x}',f'bp 0x{close_pc:x}',
             's','run 1000000000']+probes()+[f'bc 0x{close_pc:x}','s','run 1000000000']+probes()+[
             f"rb 0x{sym['_boot_error']:x} 1",f'bc 0x{exit_pc:x}',f'bp 0x{kernel_exit:x}',
             's','run 1000000000','regs','mmu',f'rb 0x{owners:x} 32','quit']
    natural_raw=n.execute('member-phase-terminal',natural,{**fixtures,'RETURN.TEST':b'1'},timeout=600)
    frames=re.split(r'\[DBG\] PC=',natural_raw)[1:]
    selected=[]
    for address in (release,close_pc,exit_pc,kernel_exit):
        matching=[frame for frame in frames if frame.startswith(f'{address:04x} ')]
        selected.append(matching[-1] if matching else '')
    cleanup_ok=False
    if all(selected):
        start,closing,ended,released=map(n.memory,selected)
        pages=[start.get(sym['_code_pages']+i,255)for i in range(bank_count)]
        fd=start.get(sym['_code_phase_fd'],255)
        final_word=lambda name:ended.get(sym[name],0)|(ended.get(sym[name]+1,0)<<8)
        cleanup_ok=(start.get(sym['_code_owned'])==bank_count and len(set(pages))==bank_count and all(32<=p<64 for p in pages)
            # At resident user entries C000 contains app data, not the kernel
            # page table. Inspect only the kernel's exit entry, before its own
            # automatic process cleanup can conceal an application leak.
            and 'c000 -> 088000' in selected[3]
            and all(released.get(owners+p-32,255)==0 for p in pages)
            and fd<128 and int(re.search(r'\bAF=([0-9a-fA-F]{4})',selected[1])[1],16)>>8==fd
            and closing.get(sym['_code_owned'])==0 and ended.get(sym['_code_owned'])==0
            and ended.get(sym['_code_phase_fd'])==255 and ended.get(sym['_boot_error'])==0
            and ended.get(sym['_test_done'])==255 and final_word('_test_failed')==0)
    checks['real terminal cleanup frees all code pages and closes the retained library before OS exit']=cleanup_ok
    checks['input artifacts unchanged']=all(n.sha(Path(p))==value for p,value in hashes.items())
    report = {'scope': __doc__, 'passed': all(checks.values()), 'checks': checks,
              'library_bank_count':bank_count,'parked_phase_banks':phase_count,
              'native_assertions_passed': word('_test_passed'), 'native_assertions_failed': word('_test_failed'),
              'capture_directory': str(n.RUN.relative_to(ROOT)), 'input_sha256': hashes,
              'library_mutations':altered,'negative_assertions':value('_test_passed')}
    (n.RUN/'member-spool-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
