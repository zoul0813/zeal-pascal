#!/usr/bin/env python3
"""Generate the relocatable unsigned arithmetic leaf used by compiled programs."""
from pathlib import Path
import re
import sys

class Engine:
    def __init__(self):self.code=bytearray();self.labels={};self.fixups=[]
    def emit(self,*values):self.code.extend(values)
    def label(self,name):assert name not in self.labels;self.labels[name]=len(self.code)
    def jump(self,name,opcode=0xc3):self.emit(opcode,0,0);self.fixups.append((len(self.code)-2,name))
    def load(self,d):self.emit(0xdd,0x7e,d)
    def store(self,d):self.emit(0xdd,0x77,d)
    def imm(self,d,v):self.emit(0xdd,0x36,d,v)
    def copy(self,src,dst,n=4):
        for i in range(n):self.load(src+i);self.store(dst+i)
    def zero(self,d,n=4):
        for i in range(n):self.imm(d+i,0)
    def shift(self,d,left):
        for i in (range(4)if left else range(3,-1,-1)):
            self.emit(0xdd,0xcb,d+i,(0x26 if left else 0x3e)if i==(0 if left else 3)else(0x16 if left else 0x1e))
    def calc(self,src,other,dst,subtract=False):
        self.emit(0xb7)
        for i in range(4):
            self.load(src+i);self.emit(0xdd,0x9e if subtract else 0x8e,other+i);self.store(dst+i)

def generate():
    e=Engine();e.emit(0xdd,0xe5,0xfd,0xe5,0xf5,0xc5,0xd5,0xe5,0x21,0xe8,0xff,0x39,0xf9,0xe5,0xdd,0xe1)
    # IX owns24 bytes. Original right HL,left DE,BC,AF,IY,IX follow it.
    for side,slot,flag,sign in [('left',0,6,4),('right',4,7,5)]:
        source=26 if side=='left'else 24
        e.load(31);e.emit(0xcb,0x47+8*flag);e.jump(side+'_wide',0xca)
        e.copy(source,slot,2);e.zero(slot+2,2)
        e.load(31);e.emit(0xcb,0x47+8*sign);e.jump(side+'_done',0xca)
        e.load(slot+1);e.emit(0xb7);e.jump(side+'_done',0xf2)
        e.imm(slot+2,255);e.imm(slot+3,255);e.jump(side+'_done')
        e.label(side+'_wide');e.emit(0xdd,0x6e,source,0xdd,0x66,source+1)
        for i in range(4):e.emit(0x7e,0x23);e.store(slot+i)
        e.label(side+'_done')
    e.load(31);e.emit(0xe6,15);e.store(17)
    operations=['add','sub','mul','div','mod','and','or','xor','shl','shr','cmp','not','copy','narrow','format','invalid']
    for op,name in enumerate(operations):
        e.load(17);e.emit(0xfe,op);e.jump(name,0xca)
    e.label('add');e.calc(0,4,8);e.jump('finish')
    e.label('sub');e.calc(0,4,8,True);e.jump('finish')
    for name,opcode in [('and',0xa6),('or',0xb6),('xor',0xae)]:
        e.label(name)
        for i in range(4):e.load(i);e.emit(0xdd,opcode,4+i);e.store(8+i)
        e.jump('finish')
    e.label('not')
    for i in range(4):e.load(4+i);e.emit(0x2f);e.store(8+i)
    e.jump('finish')
    e.label('copy');e.copy(4,8);e.jump('finish')
    e.label('mul');e.zero(8);e.emit(0x06,32);e.label('mul_loop')
    e.load(4);e.emit(0xe6,1);e.jump('mul_skip',0xca);e.calc(8,0,8)
    e.label('mul_skip');e.shift(4,False);e.shift(0,True);e.emit(0x05);e.jump('mul_loop',0xc2);e.jump('finish')
    e.label('div');e.jump('divide');e.label('mod');e.label('divide')
    e.load(4)
    for i in range(5,8):e.emit(0xdd,0xb6,i)
    e.jump('zero_error',0xca)
    e.zero(8);e.emit(0x06,32);e.label('div_loop');e.shift(0,True)
    for i in range(8,12):e.emit(0xdd,0xcb,i,0x16)
    e.emit(0x3e,0,0xce,0,0x4f);e.calc(8,4,12,True);e.jump('div_commit',0xd2)
    e.emit(0x79,0xb7);e.jump('div_skip',0xca)
    e.label('div_commit');e.copy(12,8);e.load(0);e.emit(0xf6,1);e.store(0)
    e.label('div_skip');e.emit(0x05);e.jump('div_loop',0xc2)
    e.load(17);e.emit(0xfe,4);e.jump('finish',0xca);e.copy(0,8);e.jump('finish')
    for name,left in [('shl',True),('shr',False)]:
        e.label(name);e.copy(0,8)
        e.load(5)
        for i in (6,7):e.emit(0xdd,0xb6,i)
        e.jump(name+'_zero',0xc2);e.load(4);e.emit(0xfe,32);e.jump(name+'_zero',0xd2)
        e.emit(0xb7);e.jump('finish',0xca);e.emit(0x47);e.label(name+'_loop');e.shift(8,left);e.emit(0x05);e.jump(name+'_loop',0xc2);e.jump('finish')
        e.label(name+'_zero');e.zero(8);e.jump('finish')
    e.label('cmp')
    for i in range(3,-1,-1):e.load(i);e.emit(0xdd,0xbe,4+i);e.jump('less',0xda);e.jump('greater',0xc2)
    e.emit(0x21,0,0);e.jump('scalar')
    e.label('less');e.emit(0x21,255,255);e.jump('scalar')
    e.label('greater');e.emit(0x21,1,0);e.jump('scalar')
    e.label('narrow');e.load(6);e.emit(0xdd,0xb6,7);e.jump('range_error',0xc2)
    e.emit(0xdd,0x6e,4,0xdd,0x66,5);e.jump('scalar')
    # Decimal conversion uses four-byte /10, with one byte of remainder.
    # Digits are written backwards into the caller's12-byte buffer then moved.
    e.label('format');e.emit(0xdd,0x6e,28,0xdd,0x66,29,0xe5,0xfd,0xe1)
    e.copy(4,0);e.imm(18,0);e.label('format_digit');e.emit(0x0e,0)
    for i in range(3,-1,-1):
        e.load(i);e.emit(0x57,0x1e,0,0x06,8);e.label('format_bit_'+str(i))
        e.emit(0xcb,0x22,0xcb,0x11,0x79,0xfe,10);e.jump('format_no_sub_'+str(i),0xda)
        e.emit(0xd6,10,0x4f,0x1c);e.label('format_no_sub_'+str(i));e.emit(0x05);e.jump('format_bit_end_'+str(i),0xca)
        e.emit(0xcb,0x23);e.jump('format_bit_'+str(i));e.label('format_bit_end_'+str(i));e.emit(0x7b);e.store(i)
    e.load(18);e.emit(0x5f,0x16,0,0xfd,0xe5,0xe1,0x19,0x79,0xc6,48,0x77)
    e.emit(0xdd,0x34,18);e.load(0)
    for i in range(1,4):e.emit(0xdd,0xb6,i)
    e.jump('format_digit',0xc2)
    # Reverse digits in place, then shift right by one for the short length.
    e.emit(0xfd,0xe5,0xe1);e.load(18);e.emit(0x5f,0x16,0,0x19,0x2b,0xfd,0xe5,0xd1)
    e.load(18);e.emit(0xcb,0x3f,0x47,0xb7);e.jump('format_reversed',0xca);e.label('format_reverse')
    e.emit(0x1a,0x4e,0x77,0x79,0x12,0x13,0x2b,0x05);e.jump('format_reverse',0xc2)
    e.label('format_reversed');e.emit(0xfd,0xe5,0xe1);e.load(18);e.emit(0x4f,0x06,0,0x09,0x2b,0x54,0x5d,0x13,0xed,0xb8)
    e.emit(0xfd,0xe5,0xe1);e.load(18);e.emit(0x77);e.jump('scalar')
    e.label('finish')
    for i in range(4):e.load(8+i);e.emit(0x32);e.emit_fixed('RT_REAL_BUFFER',i)
    e.jump('restore_value')
    e.label('scalar');e.emit(0x22);e.emit_fixed('RT_REAL_BUFFER')
    e.label('restore_scalar');restore(e);e.emit(0x2a);e.emit_fixed('RT_REAL_BUFFER');e.emit(0xc9)
    e.label('restore_value');restore(e);e.emit(0x21);e.emit_fixed('RT_REAL_BUFFER');e.emit(0xc9)
    for name,code in [('zero_error',2),('range_error',0x91),('invalid',255)]:
        e.label(name);e.emit(0x3e,code,0x06,2,0xc3);e.emit_fixed('RT_ERROR')
    for at,name in e.fixups:
        value=e.labels[name];e.code[at:at+2]=bytes((value&255,value>>8))
    return e

def restore(e):e.emit(0xdd,0xf9,0x21,24,0,0x39,0xf9,0xd1,0xd1,0xc1,0xf1,0xfd,0xe1,0xdd,0xe1)
def main():
    root=Path(__file__).resolve().parents[1];text=(root/'src/zp.h').read_text()
    constants={n:int(re.search(r'#define '+n+r'\s+(0x[0-9a-fA-F]+)',text)[1],16)for n in ('RT_REAL_BUFFER','RT_ERROR')}
    def fixed(self,name,delta=0):v=constants[name]+delta;self.emit(v&255,v>>8)
    Engine.emit_fixed=fixed;e=generate()
    out='/* Generated by tools/generate-wide.py. */\nstatic const uint8_t unsigned_code[]={\n'
    for at in range(0,len(e.code),20):out+=','.join(str(x)for x in e.code[at:at+20])+',\n'
    out+='};\nstatic const uint16_t unsigned_relocations[]={'+','.join(str(at)for at,_ in e.fixups)+'};\n'
    Path(sys.argv[1]).write_text(out)
if __name__=='__main__':main()
