#!/usr/bin/env python3
"""Exercise actual Pascal Inline hardware leaves on the unchanged Z80 CPU."""
from pathlib import Path
import re
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
CPU=Path(os.environ.get('ZP_CPU_ROOT','/private/tmp/Zeal-NativeEmulator'))
if not CPU.exists():CPU=ROOT.parent/'Zeal-NativeEmulator'
leaves={}
for name,procedure in {
    'transfer':'VRAMTransfer', 'write':'WritePeripheral', 'read':'ReadPeripheralRaw',
    'crc':'CRCFeed', 'tile':'GfxTilemapPlace', 'tileload':'GfxTilemapLoad',
    'palette':'GfxPaletteLoad', 'palread':'GfxPaletteRead',
    'sprite':'GfxSpriteRender', 'sprites':'GfxSpriteRenderArray',
    'setx':'GfxSpriteSetX', 'sety':'GfxSpriteSetY', 'setxy':'GfxSpriteSetXY',
    'settile':'GfxSpriteSetTile', 'setflags':'GfxSpriteSetFlags',
    'hold':'SoundSetHold', 'voices':'SoundSetVoices',
    'volume':'SoundSetVoicesVol', 'channels':'SoundSetChannels',
    'samples':'SoundPlaySamples', 'dma':'DMAStartTransfer',
}.items():
    file='ZVBIO.INC' if name in ('write','read','crc','hold','voices','volume','channels','samples','dma') else 'ZVBGFX.INC'
    text=(ROOT/'libraries'/file).read_text()
    raw=re.search(r'(?:procedure|function)\s+'+procedure+r'\b.*?Inline\((.*?)\)',text,re.S)[1]
    leaves[name]=bytes(int(v,16)for v in re.findall(r'\$([0-9A-Fa-f]{2})',raw))
with tempfile.TemporaryDirectory(prefix='zvb-leaves-')as directory:
    temp=Path(directory)
    (temp/'zvb-leaves.h').write_text(''.join('static const uint8_t '+name+'_code[]={'+','.join(map(str,data))+'};\n'for name,data in leaves.items()))
    command=['cc','-std=c99','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all',
             '-I'+str(CPU/'include'),'-I'+str(CPU/'include/utils'),'-I'+str(temp),
             str(ROOT/'tests/host/zvb-leaves.c'),str(CPU/'hw/z80.c'),'-o',str(temp/'leaves')]
    subprocess.run(command,check=True);subprocess.run([str(temp/'leaves')],check=True)
print('Inline byte counts:',{name:len(data)for name,data in leaves.items()})
