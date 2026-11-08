#!/usr/bin/env python3
"""Independently check Pascal hardware values against the C compiler's SDK values."""
import argparse,hashlib,importlib.util,json,re,subprocess,tempfile,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sdk',type=Path,required=True);a=p.parse_args()
 subprocess.run([sys.executable,str(ROOT/'tools/generate-zvb-hardware.py'),'--sdk',str(a.sdk),'--check'],check=True)
 names=[];parts=[];hashes={}
 for name in ('zvb_hardware.h','zvb_sdk.h'):
  raw=(a.sdk/'include'/name).read_text();hashes[name]=hashlib.sha256(raw.encode()).hexdigest()
  clean=re.sub(r'/\*.*?\*/|//[^\n]*','',raw,flags=re.S)
  names+=re.findall(r'^#define[ \t]+(\w+)[ \t]+[^\n]+',clean,re.M)
  def register(m):
   names.append(m[2].upper());return 'enum { '+m[2].upper()+' = '+m[1]+' };'
  clean=re.sub(r'IOB\(([^)]*)\)[ \t]+(\w+)[ \t]*;',register,clean)
  parts.append(re.sub(r'^#pragma once[^\n]*','',clean,flags=re.M))
 code='#include <stdio.h>\n'+'\n'.join(parts)+'\nint main(void){\n'+''.join('printf("'+name+'=%u\\n",(unsigned)('+name+'));\n'for name in names)+'return 0;}\n'
 with tempfile.TemporaryDirectory(prefix='zvb-defs-')as d:
  d=Path(d);(d/'values.c').write_text(code)
  subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',str(d/'values.c'),'-o',str(d/'values')],check=True)
  actual=dict((name,int(value))for name,value in(line.split('=')for line in subprocess.check_output([str(d/'values')],text=True).splitlines()))
 pas=(ROOT/'libraries/ZVBHW.INC').read_text();expected={}
 for name,value in re.findall(r'^\s*(\w+)\s*=\s*(\$[0-9A-F]+|\d+);',pas,re.M):expected[name]=int(value[1:],16)if value.startswith('$')else int(value)
 assert actual==expected,[(k,actual.get(k),expected.get(k))for k in actual.keys()|expected.keys()if actual.get(k)!=expected.get(k)]
 assert len(actual)==133
 # Keep stock-source compaction lexical; it must never rewrite quoted text.
 spec=importlib.util.spec_from_file_location('source',ROOT/'tools/zvb-source.py');source=importlib.util.module_from_spec(spec);spec.loader.exec_module(source)
 example="  const S = '{literal}(*text*)''quote'; {remove}\n  {$R-}\n(* remove *)\n{ SPDX-License-Identifier: Apache-2.0 }\n"
 assert source.authoring_source(example)=="const S = '{literal}(*text*)''quote';\n{$R-}\n{ SPDX-License-Identifier: Apache-2.0 }\n"
 text=source.source();assert len(text.encode())<=32768 and max(map(len,text.splitlines()))<=127
 report={'passed':True,'definitions':len(actual),'registers':64,'expanded_source_bytes':len(text.encode()),'sdk_header_sha256':hashes,'constant_values':actual}
 print(json.dumps(report,indent=2))
if __name__=='__main__':main()
