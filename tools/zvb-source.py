#!/usr/bin/env python3
"""Make the single-source Pascal unit consumed by normal native Unit mode."""
from pathlib import Path
import sys
import re
ROOT=Path(__file__).resolve().parents[1]
def authoring_source(text):
    """Compact only generated stock source; preserve strings, directives and licenses.

    Diagnostic offsets still refer to the exact generated file compiled on Zeal.
    The readable component files are never rewritten.
    """
    result=[];at=0
    while at<len(text):
        if text[at]=="'":
            start=at;at+=1
            while at<len(text):
                if text[at]=="'":
                    at+=1
                    if at<len(text)and text[at]=="'":at+=1;continue
                    break
                at+=1
            result.append(text[start:at]);continue
        closing='}'if text[at]=='{'else'*)'if text.startswith('(*',at)else None
        if closing:
            opening=1 if closing=='}'else 2;end=text.find(closing,at+opening)
            if end<0:raise ValueError('Unterminated stock-source comment')
            end+=len(closing);comment=text[at:end]
            result.append(comment if comment[opening:opening+1]=='$'or 'SPDX-'in comment else' ')
            at=end;continue
        result.append(text[at]);at+=1
    return '\n'.join(line.strip()for line in ''.join(result).splitlines()if line.strip())+'\n'

def source():
    text=(ROOT/'libraries/ZVB.PAS').read_text()
    for name in ('ZVBHW.INC','ZVBIO.INC','ZVBGFX.INC','ZVBPACK.INC','ZVBDECODE.INC'):
        marker='{$I '+name+'}'
        assert text.count(marker)==1,name
        text=text.replace(marker,(ROOT/'libraries'/name).read_text())
    assert '{$I ' not in text
    text=authoring_source(text)
    if max(map(len,text.splitlines()))>127:
        raise ValueError('Generated stock source line exceeds 127 characters')
    if len(text.encode())>32768:
        raise ValueError('Stock ZVB authoring exceeds its two-source-page RAM budget (32768 bytes)')
    return text
def first_bar_source():
    """Use the reference listing as the authoritative teaching example."""
    document=(ROOT/'docs/zvb-library.md').read_text()
    match=re.search(r'```pascal\n(program FirstBar;.*?)```',document,re.S)
    if not match:raise ValueError('FirstBar listing missing from the ZVB reference')
    text=match[1]
    assert text.count('{$I VIDEO.INC}')==1
    return text.replace('{$I VIDEO.INC}',(ROOT/'examples/zvb/VIDEO.INC').read_text())

def standalone_demo(path):
    """Refresh generated include sections while keeping the demo one source."""
    text=path.read_text()
    def section(name):
        # Keep the root within one source page at the native linker's RAM peak.
        # These source includes are still embedded into the one saved COM.
        if path.stem == 'SNAKE' and name in ('NUMBERS.INC','SNAKEFONT.INC'):
            return '{$I '+name+'}'
        return ('{ BEGIN GENERATED INCLUDE: '+name+' }\n'+
                (path.parent/name).read_text().rstrip()+
                '\n{ END GENERATED INCLUDE: '+name+' }')
    text=re.sub(r'(?ms)^\{ BEGIN GENERATED INCLUDE: ([A-Za-z0-9_.-]+) \}\n.*?^\{ END GENERATED INCLUDE: \1 \}',
                lambda match:section(match[1]),text)
    text=re.sub(r'(?m)^\{\$I ([A-Za-z0-9_.-]+)\} *$',lambda match:section(match[1]),text)
    assert len(text.encode())<=65534 and max(map(len,text.splitlines()))<=127
    path.write_text(text)
if __name__=='__main__':
    if sys.argv[1:]==['--demos']:
        for name in ('SNAKE','SPRITES'):standalone_demo(ROOT/'examples/zvb'/(name+'.PAS'))
        (ROOT/'tests/native/zvb/FIRSTBAR.PAS').write_text(first_bar_source())
    else:
        output=Path(sys.argv[1]);output.parent.mkdir(parents=True,exist_ok=True)
        output.write_text(source())
