#!/usr/bin/env python3
"""Independent Fraction oracles for the shared Zeal Real6 engine and formatter.

Generate native C vectors before the ZDE build; --host checks the same engine on
this host. --native executes the test artifact on the unchanged Zeal emulator.
Neither host mode executes or interprets Pascal programs.
"""
import argparse
import ctypes
from fractions import Fraction
from decimal import Decimal,localcontext
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import random
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def power(n):
    return Fraction(1<<n) if n>=0 else Fraction(1,1<<-n)

def decode(bits):
    if not bits[0]:return Fraction(0)
    m=(int.from_bytes(bits[1:],'little')&((1<<39)-1))|(1<<39)
    return (-1 if bits[5]&128 else 1)*m*power(bits[0]-168)

def encode(value):
    if not value:return bytes(6)
    sign=value<0;value=abs(value)
    n=value.numerator.bit_length()-value.denominator.bit_length()
    if value<power(n):n-=1
    e=n+129
    if e<1:return bytes(6)
    if e>255:raise OverflowError
    m=int(value/power(e-168))&((1<<39)-1)
    return bytes([e])+(m|(int(sign)<<39)).to_bytes(5,'little')

def add(left,right):
    e=max(left[0],right[0]);value=0
    for b in (left,right):
        if b[0]:
            m=(int.from_bytes(b[1:],'little')&((1<<39)-1))|(1<<39)
            value+=(-1 if b[5]&128 else 1)*(m>>(e-b[0]))
    return encode(value*power(e-168))

def expected(op,a,b,integer):
    x,y=decode(a),decode(b)
    if op==17:
        if x<0:return bytes(6),3,0
        if not x:return bytes(6),0,0
        e=(x.numerator.bit_length()-x.denominator.bit_length())//2
        scaled=x/power(2*(e-39))
        assert scaled.denominator==1
        return encode(math.isqrt(scaled.numerator)*power(e-39)),0,0
    if op==4:return bytes(6),0,(x>y)-(x<y)
    if op in (6,7):
        n=int(abs(x)+(Fraction(1,2)if op==7 else 0));n=-n if x<0 else n
        return bytes(6),0 if -32768<=n<=32767 else 0x92,n if -32768<=n<=32767 else 0
    if op==3 and not y:return bytes(6),2,0
    try:
        if op==0:r=add(a,b)
        elif op==1:r=add(a,encode(-y))
        else:r=encode({2:lambda:x*y,3:lambda:x/y,5:lambda:Fraction(integer),8:lambda:-x,
                       9:lambda:abs(x),10:lambda:Fraction(int(x)),11:lambda:x-int(x),12:lambda:x*x}[op]())
        return r,0,0
    except OverflowError:return bytes(6),1,0

def vectors(random_pairs=128):
    rng=random.Random(0x5a504153);zero=bytes(6)
    samples=[zero,bytes([0,255,255,255,255,255])]+[bytes([e])+m
        for e in (1,2,127,128,129,130,143,144,145,168,254,255)
        for m in (bytes(5),b'\xff\xff\xff\xff\x7f',b'\0\0\0\0\x80',b'\xff'*5)]
    rows=[]
    for a in samples:
        for op in (6,7,8,9,10,11,12,17):rows.append((op,a,zero,0,expected(op,a,zero,0)))
    for _ in range(random_pairs):
        a=bytes([rng.randrange(256)])+rng.randbytes(5);b=bytes([rng.randrange(256)])+rng.randbytes(5)
        for op in range(5):rows.append((op,a,b,0,expected(op,a,b,0)))
        rows.append((17,a,zero,0,expected(17,a,zero,0)))
    for n in (-32768,-32767,-1,0,1,32767):rows.append((5,zero,zero,n,expected(5,zero,zero,n)))
    # Exact cancellation, carry, alignment and normal/underflow limits.
    for x,y in [(1,3),(-1,3),(Fraction(1,3),3),(1,Fraction(1,3)),
                (1,power(-40)),(1+power(-39),power(-40)),(1,power(-41)),
                (1,-power(-40)),(1,-(1-power(-40))),
                (power(-127),2),(power(-128),2),(32767,Fraction(1,2)),(-32768,-Fraction(3,4))]:
        a,b=encode(Fraction(x)),encode(Fraction(y))
        for op in range(5):rows.append((op,a,b,0,expected(op,a,b,0)))
    return rows

DECIMALS={'0.1':'7dcccccccc4c','0.333333333333333333333333':'7fa3aaaaaa2a',
          '1.234567890123456789':'81601452061e','1.00000000000090949470177':'80feffffff7f',
          '1.00000000000272848410532':'80feffffff7f','1e-38':'0255eddcc759',
          '123456789012345678901234567890.0':'e15cfb877447','32768.0':'900000000000',
          '1.':'810000000000','1e+2':'870000000048','1.e2':'870000000048',
          '-0.1':'7dcccccccccc','0.0':'000000000000',
          '0.001E+2':'7dcccccccc4c','1.23456789E-3':'77a96c39d121',
          '1.0e37':'fbbb1ac2bd70','0.1e-38':'000000000000','0.3e-38':'01ff271eab02'}
BAD_DECIMALS=('', '.1','1e','1..0','1.2.3','1e-39','1e38','0e9999','1.0e9999999999','1 2',
              '1.0e38','0.0e38','1e001','1e000','0.'+'0'*38+'1E+1')

def array(bits):return '{'+','.join(str(n)for n in bits)+'}'

def ten(n):return Fraction(10**n) if n>=0 else Fraction(1,10**-n)

def nearest(value):
    bits=encode(value);unit=power(bits[0]-168)
    return encode(int(value/unit+Fraction(1,2))*unit)

def formatted(bits,width,places):
    """Exact rational oracle after the observed Real6 decimal normalization."""
    x=abs(decode(bits));decade=0;negative=bool(bits[0] and bits[5]&128)
    if x:
        decade=max(-38,((bits[0]-128)*77+5)//256)
        n=abs(decade);scale=nearest(ten(n//4*4))
        for _ in range(n%4):scale=encode(decode(scale)*10)
        x=decode(encode(x*decode(scale) if decade<0 else x/decode(scale)))
        if x<1:x=decode(encode(x*10));decade-=1
    sign='-' if negative else ''
    if 0<=places<=24:
        keep=min(11,decade+places+1)
        n=int(x*ten(keep-1)+Fraction(1,2))
        scaled=n*10**(decade-keep+1+places)
        digits=str(scaled).rjust(places+1,'0')
        return sign+(digits[:-places]+'.'+digits[-places:] if places else digits)
    count=max(1,min(10,width-int(bool(bits[5]&128))-6))
    n=int(x*ten(count)+Fraction(1,2))
    if n>=10**(count+1):n//=10;decade+=1
    digits=str(n).rjust(count+1,'0')
    return sign+digits[0]+'.'+digits[1:]+'E'+('+'if decade>=0 else '-')+f'{abs(decade):02d}'

def format_vectors(large=False):
    samples=[bytes(6),b'\0\xff\xff\xff\xff\xff']
    samples += [bytes([e])+mantissa for e in (1,25,26,44,70,108,128,129,132,169,210,244,255)
                for mantissa in (bytes(5),b'\xff\xff\xff\xff\x7f',b'\xff'*5)]
    fields=[(18,-1),(1,24)]
    rows=[(b,w,d,formatted(b,w,d))for b in samples for w,d in fields]
    for token in ('0.15','0.25','2.675','-0.15','0.99999999999','9.99999999999'):
        # The literal parser is tested separately; these are independent bits.
        bits=encode(Fraction(token))
        for w,d in ((1,0),(1,1),(9,-1),(1,25),(-32768,-32768),(32767,32767)):
            rows.append((bits,w,d,formatted(bits,w,d)))
    if large:
        rng=random.Random(0x5a464d54)
        for _ in range(1024):
            b=bytes([rng.randrange(256)])+rng.randbytes(5)
            for w,d in ((-32768,-1),(8,-1),(16,-1),(32767,-1),(1,0),(1,1),(1,10),(1,24)):
                rows.append((b,w,d,formatted(b,w,d)))
    return rows

def val_expected(token,real):
    """Length-delimited conversion oracle; no C parser or host floats."""
    zero=bytes(6)
    if not token:return 0,0,0,zero
    negative=token.startswith(b'-');p=int(negative)
    if not real:
        hexadecimal=token[p:p+1]==b'$';p+=int(hexadecimal);first=p;base=16 if hexadecimal else 10
        alphabet=b'0123456789abcdef' if hexadecimal else b'0123456789'
        for j in range(p,len(token)):
            if token[j:j+1].lower()not in [bytes([c])for c in alphabet]:return j+1,0,0,zero
            if int(token[first:j+1],base)>65535:return j+1,0,0,zero
        value=int(token[first:]or b'0',base)
        if not hexadecimal and value>(32768 if negative else 32767):return len(token)+1,0,0,zero
        value=(-value if negative else value)&65535
        return 0,1,value-65536 if value>=32768 else value,zero
    def digit(at):return at<len(token)and 48<=token[at]<=57
    if p<len(token)and not digit(p)and token[p]not in b'.eE':return p+1,0,0,zero
    value=zero;fraction=False;scale=0
    def scaled(value,n):
        count=abs(n);factor=nearest(ten(count//4*4))
        for _ in range(count%4):factor=encode(decode(factor)*10)
        return encode(decode(value)/decode(factor)if n<0 else decode(value)*decode(factor))
    while p<len(token):
        if token[p]==46 and not fraction:fraction=True;p+=1;continue
        if not digit(p):break
        try:value=add(encode(decode(value)*10),encode(Fraction(token[p]-48)))
        except OverflowError:return p+1,0,0,zero
        scale-=int(fraction);p+=1
    if not -38<=scale<=37:return p+1,0,0,zero
    try:value=scaled(value,scale)
    except OverflowError:return p+1,0,0,zero
    if p<len(token)and token[p]in b'eE':
        p+=1;sign=1
        if p<len(token)and token[p]in b'+-':sign=-1 if token[p]==45 else 1;p+=1
        if not digit(p):return p+1,0,0,zero
        exponent=token[p]-48;p+=1
        if digit(p):exponent=exponent*10+token[p]-48;p+=1
        scale=exponent*sign
        if not -38<=scale<=37:return p+1,0,0,zero
        try:value=scaled(value,scale)
        except OverflowError:return p+1,0,0,zero
    if p!=len(token):return p+1,0,0,zero
    if negative and value[0]:value=value[:5]+bytes([value[5]^128])
    return 0,1,0,value

def val_vectors():
    tokens=[b'',b'0',b'1',b'-1',b'+1',b'32767',b'32768',b'-32768',b'-32769',b'65535',b'65536',
            b'$FFFF',b'$8000',b'$10000',b'-$FFFF',b'+$FFFF',b'$',b'-$',b'-',b'+',b' 1',b'1 ',
            b'1\t',b'1x',b'1.0',b'.1',b'1.',b'1e2',b'1E+2',b'1e-2',b'1e',b'1e+',b'1e001',
            b'1e000',b'1.0e38',b'0.1e-38',b'0.3e-38',b'1e-39',b'0e38',b'1..0',b'1.2.3',
            b'12-3',b'1e99',b'.',b'-.',b'e2',b'-e2',b'.e2',b'$ff',b'-$ff',b'0x10',b'--1',
            b'+-1',b'1e-',b'1e+00',b'1e-00',b'1e+001',b'\0',b'1\0x',b'-\0x',b'1\x1a',b'1\r',
            b'1\n',b'0'*29+b'1x',b'0'*28+b'1x1',b'-'+b'0'*29+b'1',b'9'*39,b'1'+b'0'*38,
            b'0'*254+b'1',b'0'*254+b'x',b'0'*250+b'32768',b'-'+b'0'*249+b'32768']
    return [(15+real,t,val_expected(t,real))for t in tokens for real in (0,1)]

def c_string(data):
    return '"'+''.join(chr(c)if 32<=c<=126 and c not in (34,92)else '\\%03o'%c for c in data)+'"'

def sqrt_scalings():
    # The exact root mantissa depends on the input mantissa and exponent
    # parity. These independent integer-oracle goldens exercise every native
    # exponent without duplicating hundreds of identical five-byte rows.
    return [n.to_bytes(5,'little') for n in
            (0,1,(1<<38)-1,1<<38,(1<<39)-2,(1<<39)-1)]

def sqrt_host_vectors():
    rng=random.Random(0x53515254)
    values=[bytes([e])+m for e in range(1,256) for m in sqrt_scalings()]
    values += [bytes([rng.randrange(256)])+rng.randbytes(5) for _ in range(4096)]
    for bits in values:
        yield 17,bits,bytes(6),0,expected(17,bits,bytes(6),0)
        negative=bits[:5]+bytes([bits[5]|128])
        yield 17,negative,bytes(6),0,expected(17,negative,bytes(6),0)

def real_neighbors(value):
    if not value:return [bytes(6)]
    bits=encode(value);sign=bool(bits[5]&128)
    rank=((bits[0]-1)<<39)+(int.from_bytes(bits[1:],'little')&((1<<39)-1))
    return [bytes([(n>>39)+1])+((n&((1<<39)-1))|(int(sign)<<39)).to_bytes(5,'little')
            for n in range(rank-2,rank+3) if 0<=n<255*(1<<39)]

def wide_vectors(large=False):
    """Independent220-digit Decimal bounds, tighter than the required4 ULPs."""
    with localcontext() as context:
        context.prec=220
        if large:
            rng=random.Random(0x4c4e4558)
            values=[bytes([e])+m for e in range(256) for m in sqrt_scalings()]
            values += [bytes([rng.randrange(256)])+rng.randbytes(5)for _ in range(2048)]
            for k in range(-128,129):values += real_neighbors(Fraction(Decimal(2).ln()*k))
        else:
            values=[encode(Fraction(x))for x in (0,1,-1,2,3,4,89,-89,128,-128)]
            values += [bytes([e])+m for e in (1,88,128,129,135,255)
                       for m in (bytes(5),b'\xff\xff\xff\xff\x7f')]
            for k in (-128,127):values += real_neighbors(Fraction(Decimal(2).ln()*k))
            values += real_neighbors(Fraction(1))
        values += [b[:5]+bytes([b[5]|128])for b in values]
        for bits in values:
            x=decode(bits);dx=Decimal(x.numerator)/Decimal(x.denominator)
            for op in (18,19):
                error=0;exact=Decimal(0)
                if op==18:
                    if x<=0:error=4
                    else:exact=dx.ln()
                elif x>=89:error=1
                elif x> -89:
                    exact=dx.exp()
                    if exact>=Decimal(2)**127:error=1;exact=Decimal(0)
                    elif exact<Decimal(2)**-128:exact=Decimal(0)
                value=Fraction(exact)
                if error or not value:low=high=bytes(6)
                else:
                    magnitude=abs(value);e=magnitude.numerator.bit_length()-magnitude.denominator.bit_length()
                    if magnitude<power(e):e-=1
                    unit=power(e-39)
                    low,high=encode(value-2*unit),encode(value+2*unit)
                yield op,bits,error,low,high,value

def generate(path):
    rows=vectors(32);text='/* Generated by tools/test-real.py from exact Fraction oracles. */\n'
    text+='static const RealVector real_vectors[]={\n'
    for op,a,b,integer,(out,error,value)in rows:
        text+='    {'+f'{op},{array(a)},{array(b)},{array(out)},{error},{integer},{value}'+'},\n'
    text+='};\nstatic const DecimalVector decimal_vectors[]={\n'
    for token,bits in DECIMALS.items():text+='    {'+json.dumps(token)+','+array(bytes.fromhex(bits))+',0},\n'
    for token in BAD_DECIMALS:text+='    {'+json.dumps(token)+','+array(bytes(6))+',57},\n'
    text+='};\nstatic const FormatVector format_vectors[]={\n'
    formats=format_vectors()
    for b,w,d,wanted in formats:text+='    {'+array(b)+f',{w},{d},'+json.dumps(wanted)+'},\n'
    text+='};\nstatic const ValVector val_vectors[]={\n'
    vals=val_vectors()
    for op,token,(code,assigned,value,bits)in vals:
        text+='    {'+f'{op},{c_string(token)},{len(token)},{code},{assigned},{value},{array(bits)}'+'},\n'
    text+='};\nstatic const SqrtScaleVector sqrt_scales[]={\n'
    for mantissa in sqrt_scalings():
        odd=expected(17,bytes([129])+mantissa,bytes(6),0)[0][1:]
        even=expected(17,bytes([130])+mantissa,bytes(6),0)[0][1:]
        text+='    {'+array(mantissa)+','+array(odd)+','+array(even)+'},\n'
    text+='};\nstatic const WideVector wide_vectors[]={\n'
    for op,bits,error,low,high,_ in wide_vectors():
        text+='    {'+str(op)+','+array(bits)+','+array(low)+','+array(high)+','+str(error)+'},\n'
    text+='};\n';Path(path).write_text(text)
    print('Generated',len(rows),'primitive,',len(DECIMALS)+len(BAD_DECIMALS),'decimal and',len(formats),'format native vectors')
    print('Generated',len(vals),'Val vectors, including complete255-byte inputs')

def host():
    out=ROOT/'analysis/real-host';out.mkdir(parents=True,exist_ok=True);library=out/'real.so'
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fPIC','-shared',str(ROOT/'src/real.c'),str(ROOT/'src/transcendental.c'),'-o',str(library)],check=True)
    lib=ctypes.CDLL(str(library));a=(ctypes.c_ubyte*6).in_dll(lib,'zr_a');b=(ctypes.c_ubyte*6).in_dll(lib,'zr_b')
    result=(ctypes.c_ubyte*6).in_dll(lib,'zr_result');op=ctypes.c_ubyte.in_dll(lib,'zr_op');error=ctypes.c_ubyte.in_dll(lib,'zr_error');integer=ctypes.c_int16.in_dll(lib,'zr_integer');checks=0
    for command,left,right,n,(wanted,status,value)in vectors()+list(sqrt_host_vectors()):
        op.value=command;a[:]=left;b[:]=right;integer.value=n;lib.zr_eval()
        assert error.value==status and(status or(bytes(result)==wanted and(command not in (4,6,7)or integer.value==value))),(command,left.hex(),right.hex(),bytes(result).hex(),error.value,status)
        checks+=1
    pointer=ctypes.c_void_p.in_dll(lib,'zr_text');length=ctypes.c_uint16.in_dll(lib,'zr_length')
    for token in list(DECIMALS)+list(BAD_DECIMALS):
        buffer=ctypes.create_string_buffer(token.encode());pointer.value=ctypes.addressof(buffer);length.value=len(token);op.value=13;lib.zr_eval()
        assert error.value==(0 if token in DECIMALS else 57),(token,error.value)
        if token in DECIMALS:assert bytes(result).hex()==DECIMALS[token],(token,bytes(result).hex())
        checks+=1
    width=ctypes.c_int16.in_dll(lib,'zr_width');places=ctypes.c_int16.in_dll(lib,'zr_places')
    output=(ctypes.c_ubyte*66).in_dll(lib,'zr_formatted')
    for bits,w,d,wanted in format_vectors(large=True):
        a[:]=bits;width.value=w;places.value=d;op.value=14;lib.zr_eval()
        assert error.value==0 and output[0]<=65,(bits.hex(),w,d,error.value,output[0])
        assert bytes(output[1:output[0]+1]).decode()==wanted,(bits.hex(),w,d,bytes(output[1:output[0]+1]),wanted)
        checks+=1
    code=ctypes.c_uint16.in_dll(lib,'zr_code');assigned=ctypes.c_ubyte.in_dll(lib,'zr_assign')
    for command,token,(wanted,status,value,bits)in val_vectors():
        buffer=ctypes.create_string_buffer(token);pointer.value=ctypes.addressof(buffer);length.value=len(token)
        op.value=command;code.value=32767;assigned.value=255;error.value=255;lib.zr_eval()
        assert(error.value,code.value,assigned.value)==(0,wanted,status),(command,token,error.value,code.value,assigned.value,wanted,status)
        if status:assert integer.value==value if command==15 else bytes(result)==bits,(command,token,integer.value,bytes(result).hex(),value,bits.hex())
        checks+=1
    for command,bits,wanted,low,high,_ in wide_vectors(large=True):
        a[:]=bits;op.value=command;lib.zr_eval()
        assert error.value==wanted,(command,bits.hex(),error.value,wanted)
        if not wanted:
            actual=bytes(result)
            assert decode(low)<=decode(actual)<=decode(high),(command,bits.hex(),actual.hex(),low.hex(),high.hex())
            if not actual[0]:assert actual==bytes(6)
        checks+=1
    report={'passed':True,'host_vectors':checks,'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in(ROOT/'src/real.c',ROOT/'src/transcendental.c',ROOT/'src/real.h',ROOT/'tools/test-real.py')}}
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',checks,'host Real6 vectors')

def sanitize():
    out=ROOT/'analysis/real-sanitized';out.mkdir(parents=True,exist_ok=True)
    generate(out/'real_vectors.h')
    executable=out/'real-core'
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-sanitize-recover=all','-g',
                    '-I'+str(ROOT/'src'),'-I'+str(out),str(ROOT/'src/real.c'),str(ROOT/'src/transcendental.c'),
                    str(ROOT/'tests/native/real-core.c'),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True)

def native():
    spec=importlib.util.spec_from_file_location('n',ROOT/'tools/test-native.py');n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n);sym=n.symbols('zrealtest')
    inputs=[n.EMULATOR,n.ROM,ROOT/'build/zrealtest.bin',ROOT/'build/zrealtest.map',
            ROOT/'build/real_vectors.h',ROOT/'CMakeLists.txt',ROOT/'src/real.c',ROOT/'src/real.h',
            ROOT/'src/zp.h',ROOT/'src/transcendental.c',ROOT/'tests/native/real-core.c',ROOT/'tests/native/machine.asm',
            ROOT/'tools/test-real.py',ROOT/'tools/test-native.py']
    hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in inputs}
    stop=sym['_test_stop'];assert 0xe000<=stop<=0xe7fd
    assert 0< (ROOT/'build/zrealtest.bin').stat().st_size <49152
    for area in ('HEADER','GSINIT','GSFINAL','INITIALIZER','HOME','SYSTEM','TEXT','CODE','RODATA'):
        at,size=sym.get('s__'+area,0),sym.get('l__'+area,0)
        if size:assert 0x4000<=at<=at+size<=0xe000,(area,at,size)
    assert sym['s__DATA']+sym['l__DATA']<=0xf000
    commands=['run 100000000']+n.type_text('H:/zrealtest.bin\n')+[f'bp 0x{stop:x}','run 4000000000','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed'):commands.append(f'rb 0x{sym[name]:x} 2')
    commands+=['quit'];text=n.execute('real-primitives',commands,{'zrealtest.bin':(ROOT/'build/zrealtest.bin').read_bytes()},timeout=90);m=n.memory(text)
    word=lambda name:m.get(sym[name],0)|(m.get(sym[name]+1,0)<<8)
    unchanged=all(hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p)]for p in inputs)
    report={'passed':m.get(sym['_test_done'])==255 and word('_test_failed')==0 and unchanged,'native_assertions_passed':word('_test_passed'),'native_assertions_failed':word('_test_failed'),'capture_directory':str(n.RUN.relative_to(ROOT)),'scope':'Shared Real6 arithmetic/decimal/format and Ln/Exp engine on Zeal; direct component acceptance, not generated Pascal I/O','inputs_unchanged':unchanged,'hashes':hashes}
    (n.RUN/'real-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert report['passed']

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--generate-vectors');p.add_argument('--host',action='store_true');p.add_argument('--sanitize',action='store_true');p.add_argument('--native',action='store_true');a=p.parse_args()
    if a.generate_vectors:generate(a.generate_vectors)
    if a.host:host()
    if a.sanitize:sanitize()
    if a.native:native()
