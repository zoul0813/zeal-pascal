#!/usr/bin/env python3
"""Exercise file-based conversion, validation and missing-output recovery in ZDE."""
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
ZDE = ROOT.parent/'zeal-dev-environment/zde'


def main():
    stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')+f'-{os.getpid()}'
    folder=ROOT/'analysis/incremental-build'/stamp
    source=folder/'source';build=folder/'build'
    source.mkdir(parents=True);build.mkdir()
    for extension in ('ihx','map'):
        shutil.copyfile(ROOT/f'build/pascal_runtime.{extension}',build/f'pascal_runtime.{extension}')
    shutil.copyfile(ROOT/'tools/package-native.py',source/'package-native.py')
    (source/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.16)
project(output_rules NONE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_OBJCOPY sdobjcopy)
include("${OUTPUT_RULES}")
# A local copy lets the test edit its generator without changing project inputs.
set(zp_package_native "${CMAKE_SOURCE_DIR}/package-native.py")
add_executable(pascal_runtime IMPORTED GLOBAL)
set_target_properties(pascal_runtime PROPERTIES
    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/pascal_runtime.ihx")
zp_add_outputs(pascal_runtime WITH_EXTENSION zpl PACKAGE runtime
    EXTRA_OUTPUTS "${CMAKE_BINARY_DIR}/pascal_runtime.private"
        "${CMAKE_BINARY_DIR}/pascal_runtime.runtime.json")
''')
    records=[]
    def run(name,command,success=True):
        start=time.monotonic();log=folder/(name+'.log')
        with log.open('w')as stream:
            result=subprocess.run([str(ZDE),'--no-tty',*command],cwd=ROOT,
                                  stdout=stream,stderr=subprocess.STDOUT,check=False)
        records.append({'phase':name,'returncode':result.returncode,
                        'seconds':time.monotonic()-start})
        assert (result.returncode==0)==success,(name,log)
        return log.read_text()
    rel=lambda path:str(path.relative_to(ROOT))
    run('configure',['exec','cmake','-S','/src/'+rel(source),'-B','/src/'+rel(build),
                     '-DOUTPUT_RULES=/src/cmake/Outputs.cmake'])
    command=['cmake',rel(build),'--parallel','4']
    outputs=[build/f'pascal_runtime.{extension}'for extension in ('zpl','private','runtime.json')]
    snapshot=lambda:{p.name:(p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest())for p in outputs}
    run('first',command);original=snapshot()
    assert 'runtime:'not in run('unchanged',command)
    assert snapshot()==original
    # Both the link map and validator implementation are real generator inputs.
    for name,path in [('map-input',build/'pascal_runtime.map'),('validator-input',source/'package-native.py')]:
        path.write_text(path.read_text()+'\n')
        assert 'runtime:'in run(name,command)
        current=snapshot()
        assert {p:v[1]for p,v in current.items()}=={p:v[1]for p,v in original.items()}
        assert all(current[p][0]>original[p][0]for p in current)
        original=current
    # Every declared secondary output must be recoverable, not merely the ZPL.
    for path in outputs:
        # Delete through the same VM filesystem as the producer; host-side
        # unlink can leave stale virtiofs inode handles in sdobjcopy.
        run('remove-'+path.suffix[1:],['exec','cmake','-E','rm','-f','/src/'+rel(path)])
        assert 'runtime:'in run('recover-'+path.suffix[1:],command)
        assert {p:v[1]for p,v in snapshot().items()}=={p:v[1]for p,v in original.items()}
    mapping=build/'pascal_runtime.map';valid=mapping.read_text()
    mapping.write_text(valid.replace('runtime_prefix_end','invalid_prefix_end'))
    old_private=outputs[1].read_bytes();old_report=outputs[2].read_bytes()
    run('invalid-link-metadata',command,False)
    assert not outputs[0].exists()
    run('repeat-invalid-link-metadata',command,False)
    assert not outputs[0].exists()
    assert outputs[1].read_bytes()==old_private and outputs[2].read_bytes()==old_report
    mapping.write_text(valid);run('recover-validation',command)
    before=snapshot();assert 'runtime:'not in run('final-unchanged',command)
    assert snapshot()==before
    report={'passed':True,'scope':'Isolated standard-tool conversion fixture; no main-build files modified',
            'commands':records,'outputs_sha256':{name:data[1]for name,data in before.items()}}
    (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(folder/'report.json',flush=True)


if __name__=='__main__':main()
