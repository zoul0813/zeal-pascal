"""Read the pinned SDCC/ASxxxx Z80 objects for build-time runtime packaging.

Addresses remain area/symbol references. Only a final placement resolves them.
Unsupported records and relocation modes fail closed; this is not a general
ASxxxx linker or a target-side object reader.
"""
from dataclasses import dataclass, field
from pathlib import Path
import re


class ObjectError(ValueError):
    pass


@dataclass
class Area:
    name: str
    size: int
    absolute: bool
    address: int
    data: bytearray = field(init=False)
    initialized: set = field(default_factory=set)
    relocations: list = field(default_factory=list)

    def __post_init__(self):
        self.data = bytearray(self.size)


@dataclass(frozen=True)
class Symbol:
    name: str
    defined: bool
    value: int
    area: int | None


@dataclass(frozen=True)
class Relocation:
    site: int
    symbol: bool
    target: int
    addend: int
    kind: str


@dataclass
class Object:
    name: str
    areas: list
    symbols: list


@dataclass
class ArchiveMember:
    name: str
    object: Object


def read_object(path):
    path = Path(path)
    try:
        obj = parse_object(path.read_text())
        if obj.name == '<anonymous>':
            obj.name = path.name
        return obj
    except ObjectError as error:
        raise ObjectError(f'{path}: {error}') from error


def parse_object(text):
    lines = text.splitlines()
    if not lines or lines[0] != 'XL3':
        raise ObjectError('expected little-endian hexadecimal three-byte-address object')
    areas, symbols = [], []
    current, pending, expected, name = None, None, None, None
    for line_number, line in enumerate(lines[1:], 2):
        try:
            if line.startswith('H '):
                match = re.fullmatch(r'H ([0-9A-F]+) areas ([0-9A-F]+) global symbols', line)
                if not match or expected is not None:
                    raise ObjectError('invalid/duplicate object header')
                expected = tuple(int(x, 16) for x in match.groups())
            elif line.startswith('M '):
                if name is not None or not line[2:]:
                    raise ObjectError('invalid/duplicate module name')
                name = line[2:]
            elif line.startswith('O '):
                if line != 'O -mz80':
                    raise ObjectError('unsupported object options')
            elif line.startswith('A '):
                match = re.fullmatch(r'A (\S+) size ([0-9A-F]+) flags ([0-9A-F]+) addr ([0-9A-F]+)', line)
                if not match:
                    raise ObjectError('invalid area')
                area_name, size, flags, address = match.groups()
                size, flags, address = (int(x, 16) for x in (size, flags, address))
                if flags not in (0, 8) or size > 65535 or address + size > 65536:
                    raise ObjectError('unsupported area attributes/extent')
                if any(a.name == area_name for a in areas):
                    raise ObjectError('duplicate area')
                current = len(areas)
                areas.append(Area(area_name, size, flags == 8, address))
            elif line.startswith('S '):
                match = re.fullmatch(r'S (\S+) (Def|Ref)([0-9A-F]{6})', line)
                if not match:
                    raise ObjectError('invalid symbol')
                symbol_name, kind, value = match.groups()
                value = int(value, 16)
                if value > 65535 or (kind == 'Ref' and value):
                    raise ObjectError('invalid symbol value')
                symbols.append(Symbol(symbol_name, kind == 'Def', value,
                                      current if kind == 'Def' else None))
            elif line.startswith('T '):
                if pending is not None:
                    raise ObjectError('text without relocation record')
                pending = bytes.fromhex(line[2:])
                if len(pending) < 3:
                    raise ObjectError('short text address')
            elif line.startswith('R '):
                if pending is None:
                    raise ObjectError('relocations without text')
                _text(areas, symbols, pending, bytes.fromhex(line[2:]))
                pending = None
            else:
                raise ObjectError('unsupported object record')
        except (ValueError, IndexError) as error:
            raise ObjectError(f'line {line_number}: {error}') from error
    if pending is not None or expected != (len(areas), len(symbols)):
        raise ObjectError('incomplete object/header count mismatch')
    for symbol in symbols:
        if symbol.defined and symbol.area is not None:
            area = areas[symbol.area]
            offset = symbol.value - (area.address if area.absolute else 0)
            # End labels are valid; users of a symbol still validate its span.
            if not 0 <= offset <= area.size:
                raise ObjectError('symbol outside its area')
    return Object(name or '<anonymous>', areas, symbols)


def _text(areas, symbols, text, record):
    if len(record) < 4 or record[:2] != b'\0\0' or (len(record)-4) % 4:
        raise ObjectError('unsupported relocation record')
    area_id = int.from_bytes(record[2:4], 'little')
    if area_id >= len(areas):
        raise ObjectError('unknown text area')
    area = areas[area_id]
    start = int.from_bytes(text[:3], 'little') - (area.address if area.absolute else 0)
    edits, previous = {}, 3
    for at in range(4, len(record), 4):
        mode, site, low, high = record[at:at+4]
        if mode not in (0, 2, 9, 11, 137, 139):
            raise ObjectError(f'unsupported relocation mode {mode:02x}')
        width = 3 if mode & 1 else 2
        target = low | high << 8
        if site < previous or site + width > len(text):
            raise ObjectError('overlapping/out-of-range relocation')
        if target >= (len(symbols) if mode & 2 else len(areas)):
            raise ObjectError('unknown relocation target')
        kind = 'high8' if mode & 128 else 'low8' if mode & 1 else 'word16'
        edits[site] = (width, bool(mode & 2), target, kind)
        previous = site + width
    output, relocations, at = bytearray(), [], 3
    while at < len(text):
        if at in edits:
            width, symbol, target, kind = edits[at]
            addend = int.from_bytes(text[at:at+width], 'little')
            if addend > 65535:
                raise ObjectError('address addend exceeds Z80 range')
            relocations.append(Relocation(start+len(output), symbol, target, addend, kind))
            output.extend(bytes(2 if kind == 'word16' else 1))
            at += width
        else:
            output.append(text[at])
            at += 1
    if not 0 <= start <= start+len(output) <= area.size:
        raise ObjectError('text outside its area')
    span = set(range(start, start+len(output)))
    if span & area.initialized:
        raise ObjectError('overlapping text')
    area.initialized.update(span)
    area.data[start:start+len(output)] = output
    area.relocations.extend(relocations)


def resolve_objects(objects, area_bases, external_symbols=None):
    """Resolve objects using explicit (object-index, area-index) placements.

    Returns initialized address bytes and symbol addresses. Absolute platform
    areas cannot be moved. Uninitialized storage contributes no file bytes.
    """
    bases, definitions = {}, dict(external_symbols or {})
    spans = []
    for i, obj in enumerate(objects):
        for j, area in enumerate(obj.areas):
            base = area.address if area.absolute else area_bases[i, j]
            if not 0 <= base <= base+area.size <= 65536:
                raise ObjectError('placed area exceeds Z80 range')
            bases[i, j] = base
            if area.size:
                if any(base < end and begin < base+area.size for begin, end in spans):
                    raise ObjectError('overlapping placed areas, including uninitialized storage')
                spans.append((base, base+area.size))
        for symbol in obj.symbols:
            if not symbol.defined:
                continue
            value = symbol.value
            if symbol.area is not None and not obj.areas[symbol.area].absolute:
                value += bases[i, symbol.area]
            if symbol.name in definitions and definitions[symbol.name] != value:
                raise ObjectError(f'duplicate symbol: {symbol.name}')
            definitions[symbol.name] = value
    memory = {}
    for i, obj in enumerate(objects):
        for j, area in enumerate(obj.areas):
            data = bytearray(area.data)
            for relocation in area.relocations:
                if relocation.symbol:
                    name = obj.symbols[relocation.target].name
                    if name not in definitions:
                        raise ObjectError(f'unresolved symbol: {name}')
                    target = definitions[name]
                else:
                    target_area = obj.areas[relocation.target]
                    target = 0 if target_area.absolute else bases[i, relocation.target]
                value = target + relocation.addend
                if not 0 <= value <= 65535:
                    raise ObjectError('resolved address exceeds Z80 range')
                at = relocation.site
                if relocation.kind == 'word16':
                    data[at:at+2] = value.to_bytes(2, 'little')
                else:
                    data[at] = value >> 8 if relocation.kind == 'high8' else value & 255
            for offset in sorted(area.initialized):
                address = bases[i, j] + offset
                if address in memory:
                    raise ObjectError('overlapping placed text')
                memory[address] = data[offset]
    return memory, definitions


def read_archive(path):
    """Read existing SDK ar members without extracting or changing the SDK."""
    data = Path(path).read_bytes()
    if not data.startswith(b'!<arch>\n'):
        raise ObjectError('expected ar library')
    members, names, at = [], b'', 8
    while at < len(data):
        header = data[at:at+60]
        if len(header) != 60 or header[58:] != b'`\n':
            raise ObjectError('invalid archive member header')
        try:
            size = int(header[48:58].decode('ascii').strip())
            name = header[:16].decode('ascii').strip()
        except ValueError as error:
            raise ObjectError('invalid archive member fields') from error
        at += 60
        if size < 0 or at+size > len(data):
            raise ObjectError('archive member outside file')
        payload = data[at:at+size]
        at += size
        if size & 1:
            if data[at:at+1] != b'\n':
                raise ObjectError('invalid archive padding')
            at += 1
        if name in ('/', '/SYM64/'):
            continue
        if name == '//':
            names = payload
            continue
        if name.startswith('#1/'):
            length = int(name[3:])
            if not 0 < length <= len(payload):
                raise ObjectError('invalid inline archive name')
            name, payload = payload[:length].decode('ascii').rstrip('\0'), payload[length:]
        elif name.startswith('/'):
            offset = int(name[1:])
            end = names.find(b'/\n', offset)
            if offset < 0 or end < offset or (offset and names[offset-1:offset] != b'\n'):
                raise ObjectError('invalid archive name-table offset')
            name = names[offset:end].decode('ascii')
        else:
            name = name.removesuffix('/')
        if not name:
            raise ObjectError('empty archive member name')
        obj = parse_object(payload.decode('ascii'))
        if obj.name == '<anonymous>':
            obj.name = name
        members.append(ArchiveMember(name, obj))
    return members


def library_closure(objects, members):
    """Resolve actual symbol references; never import symbol values from a map."""
    selected = []
    definitions = {s.name for o in objects for s in o.symbols if s.defined}
    providers = {}
    for index, member in enumerate(members):
        for symbol in member.object.symbols:
            if symbol.defined and symbol.name != '.__.ABS.':
                providers.setdefault(symbol.name, []).append(index)
    pending = list(objects)
    while pending:
        obj = pending.pop()
        for area in obj.areas:
            for ref in area.relocations:
                if not ref.symbol:
                    continue
                name = obj.symbols[ref.target].name
                if name in definitions or name.startswith(('s__', 'l__')):
                    continue
                if name not in providers:
                    raise ObjectError(f'unresolved library reference: {name}')
                if len(providers[name]) != 1:
                    raise ObjectError(f'ambiguous required library symbol: {name}')
                member = providers[name][0]
                target = members[member].object
                selected.append(member)
                definitions.update(s.name for s in target.symbols if s.defined)
                pending.append(target)
    return selected


def area_symbols(objects, bases, explicit_bases=None):
    """ASxxxx generated area symbols, derived from the chosen placement."""
    groups = {}
    for i, obj in enumerate(objects):
        for j, area in enumerate(obj.areas):
            origin, size = groups.get(area.name, (None, 0))
            start = (explicit_bases or {}).get(area.name, 0) if area.absolute else bases[i, j]
            if origin is None:
                origin = start
            groups[area.name] = origin, size+area.size
    return {prefix+name:value for name,(origin,size) in groups.items()
            for prefix,value in [('s_',origin),('l_',size)]}
