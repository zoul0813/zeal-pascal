#!/usr/bin/env python3
"""Refresh a project TF card, preserving user files and verifying the real launch."""
from pathlib import Path
import gzip
import hashlib
import importlib.util
import json

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)

names = ['build/zpascal.bin', 'build/zpascal.zpl', 'build/zpascal.map', 'build/zpascal.banks.json']
snapshots = {name: (ROOT/name).read_bytes() for name in names}
hashes = {name: hashlib.sha256(data).hexdigest() for name, data in snapshots.items()}
sym = n.symbols('zpascal')
metadata = json.loads(snapshots['build/zpascal.banks.json'])
count = len(metadata['bank_bytes'])
resident, library = snapshots['build/zpascal.bin'], snapshots['build/zpascal.zpl']
identity = sym['_code_digest']-0x4000
if (len(library) < 16 or library[:6] != b'ZPL1\x01'+bytes([count])
        or resident[identity:identity+32] != hashlib.sha256(library).digest()
        or len(resident) != metadata['resident_bytes']
        or any(sym.get(f'l__BANK{i}', 0) != size for i, size in enumerate(metadata['bank_bytes'], 1))):
    raise RuntimeError('Build outputs do not match. Wait for zde cmake to finish successfully, then retry.')

image = ROOT/'build/tf.img'
previous = image.read_bytes() if image.exists() else None
base = previous if previous is not None else gzip.decompress(n.TF_TEMPLATE.read_bytes())
existing = n.zealfs_files(base)
install = {'/bin/zpascal.bin': resident, '/lib/zpascal.zpl': library}
commands = ['run 100000000']
operations = []
for directory in ('/lib/', '/bin/'):
    if directory[:-1] in existing:
        raise RuntimeError(f'T:{directory[:-1]} is a file; the project card is unchanged.')
    if directory not in existing:
        operations.append('H:/mkdir T:'+directory[:-1])
for path in install:
    if path+'/' in existing:
        raise RuntimeError(f'T:{path} is a directory; the project card is unchanged.')
    # Stock cp writes in place. On the pinned OS that can return a file-size
    # byte as an error after the first chunk. Only this disposable copy changes.
    if path in existing:
        operations.append('H:/rm T:'+path)
for path in install:
    operations.append('H:/cp H:/'+path.rsplit('/', 1)[1]+' T:'+path)
for command in operations:
    commands += n.type_text(command+'\n', ticks=2000000)+['run 2000000000']
commands += ['quit']
fixtures = {name: (n.COREUTILS/name).read_bytes() for name in ('mkdir', 'rm', 'cp')}
fixtures.update({Path(name).name: data for name, data in snapshots.items() if name.endswith(('.bin', '.zpl'))})
n.execute('project-tf-install', commands, fixtures, tf_image=base, timeout=150)
installed = (n.RUN/'project-tf-install/tf.img').read_bytes()
files = n.zealfs_files(installed)
for path, expected in install.items():
    if files.get(path) != expected:
        raise RuntimeError(f'Copy verification failed for T:{path}. The project card is unchanged. '
                           f'Capture: {n.RUN / "project-tf-install"}')
preserved = {path: data for path, data in existing.items() if path not in install}
if not all(path in files and files[path] == data for path, data in preserved.items()):
    raise RuntimeError('Existing card contents did not survive the temporary install; project card unchanged.')
if not all(n.sha(ROOT/name) == digest for name, digest in hashes.items()):
    raise RuntimeError('Build changed during packaging; project card unchanged. Retry after the build finishes.')

ready, key, work = sym['_code_ready'], sym['_key_read'], sym['_work']
commands = ['run 100000000']+n.type_text('T:/bin/zpascal.bin\n', ticks=2000000)
commands += ['run 1000000', f'bp 0x{ready:x}', 'run 2000000000', f'bc 0x{ready:x}',
             'run 10000000', f'bp 0x{key:x}', 'tap N', 'run 100000000', 'regs',
             f"rb 0x{sym['_code_owned']:x} 1", f"rb 0x{sym['_ide_state']:x} 1",
             f'rb 0x{work:x} 140', f'bc 0x{key:x}']
commands += n.type_text('q', ticks=2000000)+['quit']
raw = n.execute('project-tf-launch', commands, tf_image=installed, timeout=120)
m = n.memory(raw)
path = bytes(m.get(work+12+i, 0) for i in range(128)).split(b'\0')[0]
if (m.get(sym['_code_owned']) != count or m.get(sym['_ide_state']) != 1
        or path != b'T:/src/pascal/UNTITLED.pas'
        or (n.RUN/'project-tf-launch/tf.img').read_bytes() != installed):
    raise RuntimeError(f'Installed IDE did not pass startup validation; project card unchanged. Capture: {n.RUN}')
if not all(n.sha(ROOT/name) == digest for name, digest in hashes.items()):
    raise RuntimeError('Build changed during launch validation; project card unchanged. Retry after building.')

previous_recovery = None
if previous is not None:
    if image.read_bytes() != previous:
        raise RuntimeError('Project card changed during packaging. Close the emulator and retry; no replacement made.')
    digest = hashlib.sha256(previous).hexdigest()
    recovery = ROOT/'build/recovery'/('tf-'+digest+'.img')
    recovery.parent.mkdir(parents=True, exist_ok=True)
    if not recovery.exists():
        recovery.write_bytes(previous)
    if n.sha(recovery) != digest or image.read_bytes() != previous:
        raise RuntimeError('Recovery/image verification failed; refusing project card replacement.')
    previous_recovery = str(recovery.relative_to(ROOT))
elif image.exists():
    raise RuntimeError('A project card appeared during packaging; refusing replacement.')
image.write_bytes(installed)
report = {'passed': True, 'image': 'build/tf.img', 'image_sha256': n.sha(image),
          'previous_image_recovery': previous_recovery,
          'base_image_sha256': hashlib.sha256(base).hexdigest(),
          'preserved_existing_files': sum(data is not None for data in preserved.values()),
          'input_sha256': hashes, 'base_template_sha256': n.sha(n.TF_TEMPLATE),
          'installed_files': ['T:'+path for path in install],
          'installation_operations': operations,
          'launch_command': 'T:/bin/zpascal.bin',
          'default_library_load_and_ide_menu': True, 'startup_leaves_image_unchanged': True,
          'capture_directory': str(n.RUN.relative_to(ROOT))}
(ROOT/'build/tf.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
