#!/usr/bin/env python3
"""Validate and stage the Pascal-authored stock CRT Unit."""
import argparse
import hashlib
import json
import os
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ('units/crt/CRT.PAS', 'tools/author-crt.py')
BINDINGS = ('zpascal.bin', 'zpascal.zpl', 'pascal_runtime.profile')
STOCK = ROOT/'units/crt/prebuilt'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def publish(path, data):
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent,delete=False) as stream:
        temporary=Path(stream.name)
        try:
            stream.write(data);stream.close()
            os.chmod(temporary,0o644)
            os.replace(temporary,path)
        finally:
            temporary.unlink(missing_ok=True)


def record(directory, report_path):
    report=json.loads(report_path.read_text())
    candidate=directory/'CRT.ZPU'
    sources={name:sha(ROOT/name) for name in SOURCES}
    bindings={name:sha(directory/name) for name in BINDINGS}
    if (not report.get('passed') or not report.get('checks',{}).get('Pascal CRT compiled')
        or report.get('unit_sha256')!=sha(candidate)
        or report.get('source_sha256')!=sources['units/crt/CRT.PAS']
        or report.get('author_sha256')!=sources['tools/author-crt.py']
        or report.get('build_sha256')!=bindings):
        raise ValueError('Current successful native CRT authoring is required')
    manifest={'format':1,'unit_sha256':sha(candidate),'source_sha256':sources,
              'build_sha256':bindings,'native_authoring_report_sha256':sha(report_path)}
    publish(STOCK/'CRT.ZPU',candidate.read_bytes())
    publish(STOCK/'CRT.json',(json.dumps(manifest,indent=2)+'\n').encode())
    return manifest


def stage(directory):
    manifest=json.loads((STOCK/'CRT.json').read_text())
    if (manifest.get('format')!=1 or manifest.get('unit_sha256')!=sha(STOCK/'CRT.ZPU')
        or manifest.get('source_sha256')!={name:sha(ROOT/name) for name in SOURCES}
        or manifest.get('build_sha256')!={name:sha(directory/name) for name in BINDINGS}):
        raise ValueError('Stock CRT is stale; re-author it with the current compiler and source')
    publish(directory/'CRT.ZPU',(STOCK/'CRT.ZPU').read_bytes())
    publish(directory/'CRT.PAS',(ROOT/'units/crt/CRT.PAS').read_bytes())
    return manifest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--record-authoring',type=Path)
    args=parser.parse_args()
    manifest=record(args.directory,args.record_authoring) if args.record_authoring else stage(args.directory)
    print(json.dumps({'unit':'CRT','unit_sha256':manifest['unit_sha256'],'validated':True}))

if __name__=='__main__':main()
