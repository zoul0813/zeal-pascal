#!/usr/bin/env python3
"""Embed original Snake pixels using the installed SDK's LZ asset codec."""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def functions(path,names):
    tree=ast.parse(path.read_text());nodes=[node for node in tree.body if isinstance(node,ast.FunctionDef)and node.name in names]
    assert {node.name for node in nodes}==set(names)
    namespace={};exec(compile(ast.Module(body=nodes,type_ignores=[]),str(path),'exec'),namespace)
    return namespace

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',type=Path,default=Path(os.environ.get('ZP_ZEAL_ROOT',str(ROOT.parent)))/'Zeal-VideoBoard-SDK')
    args=parser.parse_args();out=ROOT/'examples/zvb';manifest=json.loads((out/'assets.json').read_text())
    encoder=args.sdk/'tools/zeal2gif/gif2zeal.py';decoder=args.sdk/'tools/zeal2gif/zeal2gif.py'
    compress=functions(encoder,['_lz_find_match','compress_lz'])['compress_lz']
    decompress=functions(decoder,['decompress_lz'])['decompress_lz']
    sha=lambda data:hashlib.sha256(data).hexdigest()
    for name,symbol in [('snake_tileset.zts','SnakeTiles'),('letters.zts','Letters'),('numbers.zts','Numbers')]:
        source=(args.sdk/'examples/snake/assets'/name).read_bytes();entry=manifest['assets'][name]
        assert len(source)==entry['bytes']and sha(source)==entry['sha256'],name
        pixels=source if symbol=='SnakeTiles'else bytes(pixel for value in source for pixel in(value>>4,value&15))
        # The SDK tool compresses each 16x16 tile independently.
        packed=bytes(token for at in range(0,len(pixels),256)for token in compress(list(pixels[at:at+256])))
        assert bytes(decompress(packed))==pixels,name
        text=('{ Original SDK pixels, LZ encoded. See CREDITS.md and assets.json. }\n'
              'const '+symbol+': array[0..'+str(len(packed)-1)+'] of Byte = (\n'+
              ',\n'.join('  '+','.join(str(value)for value in packed[at:at+24])for at in range(0,len(packed),24))+'\n);\n')
        assert max(map(len,text.splitlines()))<=127
        (out/entry['include']).write_text(text)
        entry.update(encoding='lz-8bpp',stored_bytes=len(packed),decoded_bytes=len(pixels),
            stored_sha256=sha(packed),decoded_sha256=sha(pixels),
            source_format='8bpp'if symbol=='SnakeTiles'else'4bpp, expanded before compression')
    # Keep ASCII tile positions through S, with original pixels for the eight
    # displayed glyphs and transparent unused slots. This is asset selection,
    # not font redesign; no omitted glyph can be displayed by Snake.
    letters=(args.sdk/'examples/snake/assets/letters.zts').read_bytes()
    selected='ACEKNORS'
    pixels=bytes(pixel for letter in range(65,84)
                 for value in (letters[(letter-65)*128:(letter-64)*128]
                               if chr(letter) in selected else bytes(128))
                 for pixel in (value>>4,value&15))
    packed=bytes(token for at in range(0,len(pixels),256)
                 for token in compress(list(pixels[at:at+256])))
    assert bytes(decompress(packed))==pixels
    (out/'SNAKEFONT.INC').write_text(
        '{ Original SDK title/score glyphs at ASCII tile IDs; unused slots transparent. }\n'
        'const SnakeFont: array[0..'+str(len(packed)-1)+'] of Byte = (\n'+
        ',\n'.join('  '+','.join(str(value) for value in packed[at:at+24])
                   for at in range(0,len(packed),24))+'\n);\n')
    manifest['snake_font']={'source':'letters.zts','glyphs':selected,
        'include':'SNAKEFONT.INC','encoding':'lz-8bpp','stored_bytes':len(packed),
        'decoded_bytes':len(pixels),'stored_sha256':sha(packed),
        'decoded_sha256':sha(pixels)}
    manifest.pop('score_letters',None)
    manifest['generator']='python3 tools/generate-zvb-assets.py --sdk <Zeal-VideoBoard-SDK>'
    manifest['encoder_sha256']=sha(encoder.read_bytes());manifest['decoder_sha256']=sha(decoder.read_bytes())
    (out/'assets.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({name:{key:entry[key]for key in('bytes','stored_bytes','decoded_bytes')}for name,entry in manifest['assets'].items()},indent=2))
if __name__=='__main__':main()
