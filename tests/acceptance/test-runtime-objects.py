#!/usr/bin/env python3
"""Check runtime-object relocation against independent SDCC-linked images."""
import hashlib
import importlib.util
import json
import os
import re
from pathlib import Path
from collections import Counter
import shlex
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
import sys
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))
from runtime_objects import (ObjectError, parse_object, read_object, resolve_objects,
                             read_archive, library_closure, area_symbols)
spec = importlib.util.spec_from_file_location('native_package', ROOT/'tools/package-native.py')
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)
SDK_LIBRARY = Path(os.environ.get('ZP_ZEAL_ROOT', str(ROOT.parent)))/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/lib/z80.lib'


def sample(mode=0, addend=b'\x34\x12', tail=b'\xc9'):
    width = 1 if mode & 1 else 2
    text = bytes(3)+b'\x21'+addend+tail
    return '\n'.join(['XL3', 'H 1 areas 2 global symbols', 'M sample',
        'S external Ref000000', f'A _TEXT size {2+width:X} flags 0 addr 0',
        'S entry Def000000', 'T '+text.hex(' '),
        'R 00 00 00 00 '+bytes([mode, 4, 0, 0]).hex(' ')])


class ObjectTests(unittest.TestCase):
    def test_area_word(self):
        obj = parse_object(sample())
        image, symbols = resolve_objects([obj], {(0, 0):0x4000})
        self.assertEqual(bytes(image.values()), b'\x21\x34\x52\xc9')
        self.assertEqual(symbols['entry'], 0x4000)

    def test_external_word_and_address_carry(self):
        obj = parse_object(sample(2, b'\x01\0'))
        image, _ = resolve_objects([obj], {(0, 0):0x6000}, {'external':0x7fff})
        self.assertEqual(bytes(image.values()), b'\x21\0\x80\xc9')

    def test_low_high_and_24bit_text_collapse(self):
        for mode, expected in [(9, 0), (137, 0x81), (11, 0), (139, 0x81)]:
            with self.subTest(mode=mode):
                obj = parse_object(sample(mode, b'\x01\0\0'))
                image, _ = resolve_objects([obj], {(0, 0):0x80ff}, {'external':0x80ff})
                self.assertEqual(bytes(image.values()), bytes([0x21, expected, 0xc9]))

    def test_nonzero_addends_and_zero_initialized_storage(self):
        obj = parse_object(sample(0, b'\xff\0'))
        image, _ = resolve_objects([obj], {(0, 0):0x8001})
        self.assertEqual(bytes(image.values()), b'\x21\0\x81\xc9')
        text = 'XL3\nH 1 areas 0 global symbols\nA _DATA size 20 flags 0 addr 0'
        image, _ = resolve_objects([parse_object(text)], {(0, 0):0x6000})
        self.assertFalse(image)  # BSS requires RAM, not bytes in the executable.

    def test_absolute_platform_symbol(self):
        text = sample(2, b'\0\0').replace('S external Ref000000', 'S external Def000008')
        image, _ = resolve_objects([parse_object(text)], {(0, 0):0x5000})
        self.assertEqual(bytes(image.values()), b'\x21\x08\0\xc9')

    def test_rejects_malformed_and_unsupported_objects(self):
        original = sample()
        cases = [original.replace('XL3', 'XH3'), original.replace('H 1', 'H 2'),
            original.replace('flags 0', 'flags 4'), original.replace('size 4', 'size 2'),
            original.replace('Def000000', 'Def000005'),
            original.replace('R 00 00 00 00 00 04 00 00', 'R 00 00 00 00 04 04 00 00'),
            original.replace('R 00 00 00 00', 'R 00 00 01 00'),
            original.replace('00 04 00 00', '00 04 01 00'),
            original.replace('00 04 00 00', '00 06 00 00'),
            original+'\nR 00 00 00 00', original+'\nP 00',
            original+'\nT 00 00 00',
            original+'\nT 00 00 00 00\nR 00 00 00 00']
        for text in cases:
            with self.subTest(text=text):
                with self.assertRaises(ObjectError):
                    parse_object(text)

    def test_unresolved_overflow_and_placement_overlap(self):
        obj = parse_object(sample(2, b'\x01\0'))
        for externals in [{}, {'external':65535}]:
            with self.assertRaises(ObjectError):
                resolve_objects([obj], {(0, 0):0x5000}, externals)
        with self.assertRaises(ObjectError):
            resolve_objects([obj, obj], {(0, 0):0x5000, (1, 0):0x5000}, {'external':0})
        bss = parse_object('XL3\nH 1 areas 0 global symbols\nA _DATA size 20 flags 0 addr 0')
        with self.assertRaises(ObjectError):
            resolve_objects([obj, bss], {(0, 0):0x5000, (1, 0):0x5002}, {'external':0})
        with self.assertRaises(ObjectError):
            resolve_objects([bss, bss], {(0, 0):0x6000, (1, 0):0x6010})

    def test_negative_addends_fail_closed_until_qualified(self):
        with self.assertRaises(ObjectError):
            parse_object(sample(11, b'\xff\xff\xff'))
        obj = parse_object(sample(2, b'\xff\xff'))
        with self.assertRaises(ObjectError):
            resolve_objects([obj], {(0, 0):0x5000}, {'external':0x5000})

    def test_generated_area_symbols(self):
        obj = parse_object(sample(2, b'\0\0').replace('external', 's__TEXT'))
        bases = {(0, 0):0x50ff}
        generated = area_symbols([obj], bases)
        self.assertEqual(generated, {'s__TEXT':0x50ff, 'l__TEXT':4})
        image, _ = resolve_objects([obj], bases, generated)
        self.assertEqual(bytes(image.values()), b'\x21\xff\x50\xc9')

    def test_archive_names_and_duplicate_member_basenames(self):
        def member(name, data):
            header = f'{name:<16}{0:<12}{0:<6}{0:<6}{0:<8}{len(data):<10}`\n'.encode()
            return header+data+(b'\n' if len(data) & 1 else b'')
        obj = sample().encode()
        table = b'a_long_runtime_member.rel/\n'
        archive = b'!<arch>\n'+member('/', b'index')+member('//', table)
        archive += member('/0', obj)+member('same.rel/', obj)+member('same.rel/', obj)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'sample.lib'
            path.write_bytes(archive)
            objects = read_archive(path)
            self.assertEqual([m.name for m in objects],
                ['a_long_runtime_member.rel', 'same.rel', 'same.rel'])
            for data in [b'bad', archive[:-1], archive.replace(b'/0              ', b'/1              ', 1)]:
                path.write_bytes(data)
                with self.assertRaises((ObjectError, ValueError)):
                    read_archive(path)

    def test_library_closure_is_transitive_and_ignores_unused_declarations(self):
        from runtime_objects import ArchiveMember
        root = parse_object(sample(2, b'\0\0'))
        a = parse_object(sample(2, b'\0\0').replace('external', 'helper').replace('entry', 'external'))
        b = parse_object(sample().replace('entry', 'helper'))
        members = [ArchiveMember('a.rel', a), ArchiveMember('b.rel', b)]
        self.assertEqual(library_closure([root], members), [0, 1])
        with self.assertRaises(ObjectError):
            library_closure([root], members[:1])
        with self.assertRaises(ObjectError):
            library_closure([root], members+[ArchiveMember('duplicate.rel', a)])


def linked_oracle(target):
    """Use independently produced link maps/HEX, not this parser's own layout."""
    directory = ROOT/'build'
    tokens = shlex.split((directory/f'CMakeFiles/{target}.dir/link.txt').read_text())
    paths = [directory/token for token in tokens if token.endswith('.rel')]
    objects = [read_object(path) for path in paths]
    members = read_archive(SDK_LIBRARY)
    closure = library_closure(objects, members)
    mapping = (directory/(target+'.map')).read_text()
    linked_members = (re.findall(r'^\s+\[ ([^\]\n]+\.rel) \]',
                      mapping.split('Libraries Linked', 1)[1], re.M)
                      if 'Libraries Linked' in mapping else [])
    assert Counter(linked_members) == Counter(members[i].name for i in closure), (target, linked_members, closure)
    for name in linked_members:
        candidates = [i for i in closure if members[i].name == name]
        assert len(candidates) == 1, (target, 'ambiguous member order', name)
        objects.append(members[candidates[0]].object)
    symbols = native.read_symbols(directory/(target+'.ihx'))
    offsets, bases = {}, {}
    for i, obj in enumerate(objects):
        for j, area in enumerate(obj.areas):
            if not area.absolute:
                bases[i, j] = symbols['s_'+area.name] + offsets.get(area.name, 0)
                offsets[area.name] = offsets.get(area.name, 0) + area.size
    explicit = {}
    for i, token in enumerate(tokens):
        if token == '-b':
            name, value = tokens[i+1].split('=')
            explicit[name] = int(value, 0)
    generated = area_symbols(objects, bases, explicit)
    image, definitions = resolve_objects(objects, bases, generated)
    expected = native.read_hex(directory/(target+'.ihx'))
    assert set(image) == set(expected), (target, 'missing', len(set(expected)-set(image)),
                                       'extra', len(set(image)-set(expected)))
    for address, value in image.items():
        if expected.get(address) != value:
            raise AssertionError(f'{target}: address {address:04x}: {value:02x} != {expected.get(address)}')
    checked = 0
    for obj in objects:
        for symbol in obj.symbols:
            if symbol.defined and symbol.name in symbols:
                assert definitions[symbol.name] == symbols[symbol.name], (target, symbol.name)
                checked += 1
    kinds = {name:sum(r.kind == name for o in objects for a in o.areas for r in a.relocations)
             for name in ('word16','low8','high8')}
    modes = {f'{kind}_{target}':sum(r.kind == kind and r.symbol == (target == 'symbol')
              for o in objects for a in o.areas for r in a.relocations)
              for kind in ('word16','low8','high8') for target in ('area','symbol')}
    return {'target':target, 'objects':len(objects), 'library_members':linked_members,
            'expected_initialized_bytes':len(expected), 'initialized_bytes_checked':len(image),
            'defined_symbols_checked':checked,
            'relocation_kinds':kinds,
            'relocation_modes':modes,
            'relocations_checked':sum(len(a.relocations) for o in objects for a in o.areas),
            'input_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                for p in paths+[directory/(target+'.ihx'),directory/(target+'.map')]},
            'sdk_library_sha256':hashlib.sha256(SDK_LIBRARY.read_bytes()).hexdigest()}


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ObjectTests)
    result = unittest.TextTestRunner(verbosity=1).run(suite)
    if not result.wasSuccessful():
        raise SystemExit(1)
    oracles = [linked_oracle(name) for name in ('pascal_runtime','integer_runtime',
        'compact_runtime','file_runtime','runtime_object_a','runtime_object_b')]
    print(json.dumps({'passed':True, 'unit_tests':result.testsRun, 'linked_oracles':oracles,
        'source_sha256':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
          for name in ('tools/runtime_objects.py','tests/acceptance/test-runtime-objects.py',
            'tools/package-native.py','CMakeLists.txt','tests/native/runtime-objects.c',
            'tests/native/runtime-object-entry.asm')}}, indent=2))
