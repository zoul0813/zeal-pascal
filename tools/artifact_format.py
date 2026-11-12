"""Independent host checks for native COM/CHN files; no source-map sidecars."""
import struct

def word(data,at):return struct.unpack_from('<H',data,at)[0]

def validate_image(image,chain=False):
    if chain:
        assert image[:10]==b'ZCHN\2\0\x60\0\1\0'
        assert len(image)==96+word(image,12) and image[20:96]==bytes(76)
        origin,size=word(image,10),word(image,12)
    else:
        assert image[0]==0xc3 and image[3:8]==b'ZP80\2'
        assert word(image,8)==160 and len(image)==word(image,10)
        assert image[36:132]==bytes(96)
        origin,size=word(image,20),word(image,22)
        flags=word(image,18)
        assert not flags & ~7 and flags & 6 != 6
        assert word(image,24)+word(image,26)<=origin
        if flags & 6:
            assert word(image,30)==0xfbff and word(image,28)==0x4000+len(image)
            floor=(word(image,28)+1)&~1
            ceiling=0x8000 if flags & 4 else 0xc000
            if flags & 2:floor=max(floor,0x8000)
            assert floor+word(image,32)+1024<=ceiling
    assert size and (0x8000 if chain else 0x40a0)<=origin<origin+size<=0xf400
    return {'origin':origin,'code_bytes':size,'file_bytes':len(image)}
