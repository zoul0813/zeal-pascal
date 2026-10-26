#!/usr/bin/env python3
"""Prepare a separate project TF card and verify default-library launch on Zeal."""
from pathlib import Path
import gzip,hashlib,importlib.util,json,shutil
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',root/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
names=['build/zpascal.bin','build/zpascal.zpl']
hashes={name:n.sha(root/name)for name in names}
image=root/'build/tf.img'
previous=image.read_bytes()if image.exists()else None
base=previous if previous is not None else gzip.decompress(n.TF_TEMPLATE.read_bytes())
existing=n.zealfs_files(base)
commands=['run 100000000']
for command in ('H:/mkdir T:/lib','H:/mkdir T:/bin',
                'H:/cp H:/zpascal.bin T:/bin/zpascal.bin',
                'H:/cp H:/zpascal.zpl T:/lib/zpascal.zpl'):
 commands+=n.type_text(command+'\n',ticks=2000000)+['run 2000000000']
commands+=['quit']
n.execute('project-tf-install',commands,
          {name:(n.COREUTILS/name).read_bytes()for name in ('mkdir','cp')},
          tf_image=base,timeout=150)
installed=(n.RUN/'project-tf-install/tf.img').read_bytes()
files=n.zealfs_files(installed)
assert files['/bin/zpascal.bin']==(root/'build/zpascal.bin').read_bytes()
assert files['/lib/zpascal.zpl']==(root/'build/zpascal.zpl').read_bytes()
preserved={path:data for path,data in existing.items()
           if path not in ('/bin/zpascal.bin','/lib/zpascal.zpl')}
assert all(path in files and files[path]==data for path,data in preserved.items())
sym=n.symbols('zpascal');ready,key,work=sym['_code_ready'],sym['_key_read'],sym['_work']
commands=['run 100000000']+n.type_text('T:/bin/zpascal.bin\n',ticks=2000000)
commands+=['run 1000000',f'bp 0x{ready:x}','run 2000000000',f'bc 0x{ready:x}',
           'run 10000000',f'bp 0x{key:x}','tap N','run 100000000','regs',
           f"rb 0x{sym['_code_owned']:x} 1",f"rb 0x{sym['_ide_state']:x} 1",
           f'rb 0x{work:x} 140',f'bc 0x{key:x}']
commands+=n.type_text('q',ticks=2000000)+['quit']
raw=n.execute('project-tf-launch',commands,tf_image=installed,timeout=120)
m=n.memory(raw)
path=bytes(m.get(work+12+i,0)for i in range(128)).split(b'\0')[0]
assert m.get(sym['_code_owned'])==n.code_banks()
assert m.get(sym['_ide_state'])==1
assert path==b'T:/src/pascal/UNTITLED.pas'
assert (n.RUN/'project-tf-launch/tf.img').read_bytes()==installed
assert all(n.sha(root/name)==digest for name,digest in hashes.items())
previous_recovery=None
if previous is not None:
 assert image.read_bytes()==previous, 'Project TF image changed during packaging; refusing replacement'
 digest=hashlib.sha256(previous).hexdigest()
 recovery=root/'build/recovery'/('tf-'+digest+'.img')
 recovery.parent.mkdir(parents=True,exist_ok=True)
 if not recovery.exists(): recovery.write_bytes(previous)
 assert n.sha(recovery)==digest
 assert image.read_bytes()==previous, 'Project TF image changed during recovery; refusing replacement'
 previous_recovery=str(recovery.relative_to(root))
else:
 assert not image.exists(), 'Project TF image appeared during packaging; refusing replacement'
(root/'build/tf.img').write_bytes(installed)
report={'passed':True,'image':'build/tf.img','image_sha256':n.sha(root/'build/tf.img'),
 'previous_image_recovery':previous_recovery,
 'base_image_sha256':hashlib.sha256(base).hexdigest(),
 'preserved_existing_files':sum(data is not None for data in preserved.values()),
 'input_sha256':hashes,'base_template_sha256':n.sha(n.TF_TEMPLATE),
 'installed_files':['T:/bin/zpascal.bin','T:/lib/zpascal.zpl'],
 'launch_command':'T:/bin/zpascal.bin',
 'default_library_load_and_ide_menu':True,'startup_leaves_image_unchanged':True,
 'capture_directory':str(n.RUN.relative_to(root))}
(root/'build/tf.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
