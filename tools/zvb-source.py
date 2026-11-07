#!/usr/bin/env python3
"""Make the single-source Pascal unit consumed by normal native Unit mode."""
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
def source():
    text=(ROOT/'libraries/ZVB.PAS').read_text()
    for name in ('ZVBIO.INC','ZVBGFX.INC','ZVBPACK.INC'):
        marker='{$I '+name+'}'
        assert text.count(marker)==1,name
        text=text.replace(marker,(ROOT/'libraries'/name).read_text())
    assert '{$I ' not in text
    return text
if __name__=='__main__':
    output=Path(sys.argv[1]);output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(source())
