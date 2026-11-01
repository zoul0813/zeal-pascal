"""Independent host checks for version-2 native artifacts (no target hashes)."""
import struct


def word(data, at):
    return struct.unpack_from('<H', data, at)[0]


def source_records(mapping):
    assert mapping[:10] == b'ZMAP\2\0\x60\0\1\0'
    assert mapping[20:84] == bytes(64) and mapping[92:96] == bytes(4)
    table = struct.unpack_from('<I', mapping, 84)[0]
    entries = struct.unpack_from('<I', mapping, 16)[0]
    assert len(mapping) == 96 + table + entries * 16
    assert struct.unpack_from('<I', mapping, 88)[0] == entries * 16
    records = {}; paths = set(); at = 96
    while at < 96 + table:
        assert at + 8 <= 96 + table
        file, size, length = struct.unpack_from('<HHI', mapping, at)
        assert 0 < size <= 127 and at + 8 + size <= 96 + table
        path = mapping[at+8:at+8+size]
        assert b'\0' not in path and file not in records and path not in paths
        records[file] = (path, length); paths.add(path); at += 8 + size
    assert at == 96 + table and set(records) == set(range(word(mapping, 14)))
    previous = 0
    for at in range(96 + table, len(mapping), 16):
        first, last, file, reserved, start, end = struct.unpack_from('<4H2I', mapping, at)
        assert previous <= first < last <= word(mapping, 12) and not reserved
        assert file in records and start <= end <= records[file][1]
        previous = last
    return records


def validate_pair(image, mapping, chain=False):
    if chain:
        assert image[:10] == b'ZCHN\2\0\x60\0\1\0'
        assert len(image) == 96 + word(image, 12) and image[20:96] == bytes(76)
        origin, size = word(image, 10), word(image, 12)
    else:
        assert image[0] == 0xc3 and image[3:8] == b'ZP80\2'
        assert word(image, 8) == 160 and len(image) == word(image, 10)
        assert image[36:132] == bytes(96)
        origin, size = word(image, 20), word(image, 22)
    assert (origin, size) == (word(mapping, 10), word(mapping, 12))
    return source_records(mapping)
