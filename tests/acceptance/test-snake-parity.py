#!/usr/bin/env python3
"""Compare saved Pascal Snake with the unchanged SDK C game in zeal-native.

Observe actual VRAM transfers, raw keyboard events and OS exit. Test fixtures
use the linked globals address from the executable header, never a fixed base.
"""
import argparse
import hashlib
import importlib.util
import json
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snake',type=Path,default=ROOT/'build/zvb/SNAKE.COM')
    parser.add_argument('--emulator',type=Path,default=Path('/opt/homebrew/bin/zeal-native'))
    args=parser.parse_args()
    spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
    n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    n.EMULATOR=args.emulator
    image=args.snake.read_bytes()
    reference=json.loads((ROOT/'tests/fixtures/snake/c-observation.json').read_text())
    pixels=json.loads((ROOT/'tests/fixtures/snake/reference-pixels.json').read_text())
    assert not reference['failure'] and not pixels['failure']
    declaration=(ROOT/'examples/zvb/SNAKE.PAS').read_text().split(
        'var Context: TGfxContext;',1)[1].split('{ BEGIN GENERATED INCLUDE: VIDEO.INC }',1)[0]
    assert ' '.join(declaration.split())==' '.join('''
      { Zero is empty;1..4 links an occupied cell toward the head. }
      Board: array[0..279] of Byte;
      TailX, TailY, HeadX, HeadY: Byte;
      SnakeLength, Score: Integer;
      Frames: Word;
      Direction, AppleX, AppleY, Tick: Byte;
      Error: Byte;
      GameOver, Quit, Demo, Waiting, JustAte, Controller: Boolean;
      Key: Char;
      Speed: Byte;
      Keys: array[0..31] of Byte;
      KeyCount: Word;
      InputError: Byte;
    '''.split()),'Update the observer when Snake declarations change'
    base=int.from_bytes(image[24:26],'little')+5+280

    def site(pattern,offset=0):
        value=bytes.fromhex(pattern)
        assert image.count(value)==1,pattern
        return 0x4000+image.index(value)+offset

    # Locate the actual public Inline leaves linked into this executable.
    # A restore tail may occur in many functions; locate it within its body.
    mappings={};restores=set()
    gfx=(ROOT/'units/zvb/ZVBGFX.INC').read_text()
    for method in ('VRAMTransfer','GfxTilemapPlace','GfxTilemapLoad',
            'GfxPaletteLoad','GfxPaletteRead','GfxSpriteRender','GfxSpriteRenderArray',
            'GfxSpriteSetX','GfxSpriteSetY','GfxSpriteSetXY',
            'GfxSpriteSetTile','GfxSpriteSetFlags'):
        raw=re.search(r'(?:procedure|function)\s+'+method+r'\b.*?Inline\((.*?)\)',gfx,re.S)[1]
        code=bytes(int(v,16)for v in re.findall(r'\$([0-9A-Fa-f]{2})',raw))
        if code not in image:continue  # unused API was omitted by static linking
        assert image.count(code)==1,method
        start=0x4000+image.index(code)
        mappings[start+code.index(bytes.fromhex('d3f0'))]=method
        restores.add(start+code.rindex(bytes.fromhex('f1d3f0')))
    assert 'GfxSpriteSetY' in mappings.values()
    tile_map=next(at for at,method in mappings.items() if method=='GfxTilemapPlace')
    prologue=bytes.fromhex('dde5dd210000dd39')
    tile_entry=0x4000+image.rfind(prologue,0,tile_map-0x4000)
    assert tile_entry>=0x4000
    sleep_call=site('211200cf',3)

    poll=site('dd6e06dd66074e2346dd5e08dd5609210001cf')
    checks,observations,title={},{},[]
    title_cycles=[]
    sha=lambda data:hashlib.sha256(data).hexdigest()

    class Snake(n.DebugConsole):
        def command(self,command,capture=False):
            if command!='@parity':return super().command(command,capture)
            call=lambda value:super(Snake,self).command(value)
            vram={page:bytearray(16384)for page in (64,68,69)}
            pending=None
            wipe=[]
            before_wipe=None
            wipe_start=None
            actual_title=[]
            place_return=None;place_start=None
            place_costs=[];sleep_costs=[];sleep_start=None


            def read(at,size):
                data={}
                for start in range(0,size,256):
                    data.update(n.memory(call(f'rb 0x{at+start:x} {min(256,size-start)}')))
                return bytes(data[at+i]for i in range(size))

            def regs():
                return {name:int(value,16)for name,value in re.findall(
                    r'\b(PC|SP|IX|BC|DE|HL)=([0-9a-fA-F]+)',call('regs'))}

            def sample():
                data=read(base,23)
                word=lambda i:int.from_bytes(data[i:i+2],'little')
                return dict(head=data[2]+20*data[3],tail=data[0]+20*data[1],
                    length=word(4),score=word(6),frames=word(8),direction=data[10],
                    apple=[data[11],data[12]],tick=data[13],game_over=data[15],
                    quit=data[16],demo=data[17],waiting=data[18],just_ate=data[19],
                    controller=data[20],key=data[21],speed=data[22])

            def cycles():
                # The installed console reports cycles on step, not on run.
                return int(re.search(r'cyc=(\d+)',call('s'))[1])

            def run_to(address):
                nonlocal pending,before_wipe,wipe_start,place_return,place_start,sleep_start
                for _ in range(3000):
                    call('run 2000000000');state=regs();pc=state['PC']
                    if pc==tile_entry:
                        if sample()['game_over'] and not sample()['waiting']:
                            assert place_return is None
                            place_return=int.from_bytes(read(state['SP'],2),'little')
                            call(f'bp 0x{place_return:x}')
                            place_start=cycles()
                        else:call('s')
                    elif place_return is not None and pc==place_return:
                        place_costs.append(cycles()-place_start)
                        call(f'bc 0x{place_return:x}');place_return=None
                    elif pc==sleep_call:
                        if sample()['game_over'] and not sample()['waiting']:
                            sleep_start=cycles()
                        else:call('s')
                    elif pc==sleep_call+1:
                        if sleep_start is not None:sleep_costs.append(cycles()-sleep_start);sleep_start=None
                        else:call('s')
                    elif pc in mappings:
                        method=mappings[pc]
                        call('s')  # Execute OUT(F0); stop before the copy/store.
                        page=read(state['IX']+14,1)[0] if method=='VRAMTransfer' else 64
                        if method=='GfxTilemapPlace':offset,count=state['HL'],1
                        elif method in ('GfxSpriteSetX','GfxSpriteSetY','GfxSpriteSetTile','GfxSpriteSetFlags'):
                            offset,count=state['HL'],2
                        elif method=='GfxSpriteSetXY':offset,count=state['HL']-2,4
                        elif method=='GfxPaletteRead':offset,count=state['HL'],state['BC']
                        else:offset,count=state['DE'],state['BC']
                        pending=(page,offset,count)
                        if page==64 and offset==0xe0c and sample()['game_over']:
                            before_wipe=bytes(vram[64][0x1000:0x14b0])
                            wipe_start=cycles()
                        if address=='map':
                            state['_method']=method
                            return state
                    elif pc in restores:
                        assert pending is not None
                        page,offset,count=pending
                        if page in vram:
                            assert offset+count<=16384
                            data=read(offset,count)
                            vram[page][offset:offset+count]=data
                            if page==64 and count==2 and 0x2800<=offset<0x2830 and offset%8==0:
                                y=int.from_bytes(data,'little')
                                if y!=480:actual_title.append([(offset-0x2800)//8,y])
                            game=sample()
                            if page==64 and count==1 and 0x1000<=offset<0x1460 and game['game_over'] and not game['waiting']:
                                wipe.append(offset-0x1000)
                        pending=None
                        call('s')
                    else:
                        assert pc==address,state
                        return state
                raise AssertionError('observer did not reach requested checkpoint')

            for at in set(mappings)|restores|{tile_entry,sleep_call,sleep_call+1}:call(f'bp 0x{at:x}')
            for _ in range(1400):
                state=run_to('map')
                page,offset,count=pending
                if state['_method']=='GfxSpriteSetY' and 0x2800<=offset<0x2830:
                    y=state['DE']
                    if y!=480:
                        title.append([(offset-0x2800)//8,y])
                        title_cycles.append(cycles())
                        if len(title)==100:call('tap UP')
                        if len(title)==len(reference['title_trace'])+6:break
            else:raise AssertionError('title trace incomplete')
            checks['title Y calls match original C']=title[:-6]==reference['title_trace']
            checks['title cadence matches C']=all(abs((title_cycles[i+75]-title_cycles[i])-(reference['title_cycles'][i+75]-reference['title_cycles'][i]))<10000 for i in range(0,375,75))
            checks['all title sprites hide']=title[-6:]==[[i,0]for i in range(6)]
            sprites=read(0x2800,48)
            checks['original title X tile flags options']=all(
                int.from_bytes(sprites[i*8+2:i*8+4],'little')==128+16*i and
                sprites[i*8+4:i*8+8]==bytes([tile,0,0,0])
                for i,tile in enumerate([83,78,65,75,69,0]))
            checks['original checkerboard and dark strip']=list(read(0,1200))==reference['observations']['board']
            checks['bare foreground at end of title']=all(
                read(0x1000+y*80,20)==bytes([15])*20 for y in range(15))
            checks['native snake pixels match C']=sha(vram[68][:4096])==pixels['snake_sha256']
            checks['generated checker tile pixels match C']=sha(vram[68][0x2000:0x2300])==pixels['checker_sha256']
            checks['native number pixels match C']=sha(vram[68][0x3000:0x3b00])==pixels['numbers_sha256']
            checks['native title and score font pixels match C']=all(
                sha(vram[69][(ord(letter)-64)*256:(ord(letter)-63)*256])==expected
                for letter,expected in pixels['font_sha256'].items())
            checks['native font palette matches C']=list(vram[64][0xe40:0xe4c])==pixels['font_palette']
            # Check every independent sound leaf, not just the generic writer.
            io=(ROOT/'units/zvb/ZVBIO.INC').read_text()
            sound_leaves=[]
            for method in ('WritePeripheral','SoundSetHold','SoundSetVoices',
                    'SoundSetVoicesVol','SoundSetChannels','SoundPlaySamples'):
                raw=re.search(r'procedure\s+'+method+r'\b.*?Inline\((.*?)\)',io,re.S)[1]
                sound_leaves.append(bytes(int(v,16)for v in re.findall(r'\$([0-9A-Fa-f]{2})',raw)))
            checks['all sound writers omitted']=all(code not in image for code in sound_leaves)
            call(f'bp 0x{poll:x}');call('s');run_to(poll)
            initial=sample();observations['initial']=initial
            checks['title coordinates written to actual VRAM']=actual_title==title
            checks['automatic start']=initial['head']==150 and initial['tail']==149 and initial['frames']==0 and not initial['waiting'] and not initial['demo']

            def next_frame():
                before=sample()['frames']
                for _ in range(12):
                    call('s');run_to(poll);state=sample()
                    if state['frames']!=before:return state
                raise AssertionError('frame did not advance')

            def write(at,data):
                # The installed console accepts six byte arguments per command.
                for start in range(0,len(data),6):
                    call(f'wb 0x{at+start:x} '+' '.join(map(str,data[start:start+6])))

            def apple(x,y):call(f'wb 0x{base+11:x} {x} {y}')

            state=next_frame()
            checks['title-buffered arrow steers on first frame']=state['direction']==0 and state['head']==150
            call(f'wb 0x{base+10:x} 1');apple(11,7)
            while state['frames']<20:state=next_frame()
            observations['eat']=state
            checks['first move defers growth and score']=state['head']==151 and state['tail']==150 and state['score']==0 and state['length']==2 and state['just_ate']==1
            apple(0,13)  # Avoid a random second apple on the next test cell.
            while state['frames']<40:state=next_frame()
            observations['growth']=state
            checks['next move commits growth and score']=state['head']==152 and state['tail']==150 and state['score']==1 and state['length']==3 and not state['just_ate']
            growth=bytes(vram[64][0x1000:0x14b0])
            checks['original head straight body and tail tiles']=growth[7*80+10:7*80+13]==bytes([11,12,1])
            call('tap UP');state=next_frame();state=next_frame()
            checks['arrow changes direction before movement']=state['direction']==0 and state['head']==152
            call('tap LEFT');state=next_frame();state=next_frame()
            checks['C quick-turn buffering']=state['direction']==3 and state['head']==152
            while not state['game_over']:state=next_frame()
            observations['death']=state
            checks['failed movement leaves rendered snake unchanged']=state['head']==152 and before_wipe==growth
            end=cycles()
            observations['wipe_tstates']=end-wipe_start
            observations['C_wipe_tstates']=pixels['wipe_tstates']
            observations['wipe_profile']={
                'tile_calls':len(place_costs),'tile_call_tstates':sum(place_costs),
                'sleep_calls':len(sleep_costs),'sleep_syscall_tstates':sum(sleep_costs),
                'other_tstates':end-wipe_start-sum(place_costs)-sum(sleep_costs)}
            checks['wipe duration within two video frames of C']=abs(observations['wipe_tstates']-pixels['wipe_tstates'])<333334
            checks['apple wipe runs in original row-major order']=wipe==[y*80+x for y in range(14)for x in range(20)]
            checks['death palette matches observed C']=list(vram[64][0xe00:0xe26])==reference['observations']['palette'][:38]
            checks['original score glyphs and placement']=vram[64][0x146a:0x1473]==bytes([83,67,79,82,69,58,48,48,49])
            while not state['waiting']:
                call('s');run_to(poll);state=sample()
            call('tap R');state=next_frame();observations['replay']=state
            checks['fresh replay restores score and initial state']=state['head']==150 and state['score']==0 and not state['waiting'] and not state['game_over'] and state['direction']==1 and state['tick']==0
            checks['replay restores original palette']=list(vram[64][0xe00:0xe26])==[31,220,0,0,192,8,161,138,98,83,64,218,155,75,223,91,255,91,195,5,127,108,220,124,131,102,133,94,191,157,190,247,168,174,39,158,68,108]
            checks['replay clears apple field']=all(vram[64][0x1000+y*80:0x1000+y*80+20]==bytes([15])*20 for y in range(14))
            # Eat in a straight line to exercise the eighth-score speed boost.
            for _ in range(9):
                old=state['head'];apple(old%20+1,old//20)
                while state['head']==old and not state['game_over']:state=next_frame()
            observations['speed_boost']=state
            checks['eight scores raise speed']=state['score']==8 and state['speed']==1 and state['head']==159
            frame=state['frames'];old=state['head'];call('tap W')
            while state['head']==old and not state['game_over']:state=next_frame()
            observations['corner']=state
            checks['WASD and faster move cadence']=state['direction']==0 and state['head']==139 and state['frames']-frame==19
            checks['original corner tile']=vram[64][0x1000+7*80+19]==4
            call('tap RIGHT');state=next_frame();state=next_frame()
            checks['right arrow accepted']=state['direction']==1
            call('tap DOWN');state=next_frame();state=next_frame()
            checks['down arrow accepted']=state['direction']==2
            # A consistent two-cell fixture near the bottom tests widened indices.
            board=base-280
            write(board,[0]*280)
            call(f'wb 0x{board+249:x} 2 2')
            write(base,[9,12,10,12,2,0,0,0])
            call(f'wb 0x{base+10:x} 2 10 13 19')
            call(f'wb 0x{base+19:x} 0')
            state=next_frame();observations['bottom_row']=state
            checks['bottom row index exceeds Byte safely']=state['head']==270 and state['tail']==250 and not state['game_over']
            while not state['game_over']:state=next_frame()
            checks['bottom wall collision']=state['head']==270
            checks['pending growth scores even on fatal move']=state['score']==1 and state['length']==3
            while not state['waiting']:
                call('s');run_to(poll);state=sample()
            # Kernel exit is distinct from an ordinary read syscall.
            exit_at=n.kernel_symbols()['zos_loader_exit']
            for at in set(mappings)|restores|{poll,tile_entry,sleep_call,sleep_call+1}:call(f'bc 0x{at:x}')
            call(f'bp 0x{exit_at:x}');call('tap ESC');call('s')
            call('run 10000000')
            exit_state=regs()
            checks['Escape invokes actual successful process exit']=exit_state['PC']==exit_at and exit_state['HL']>>8==0
            call(f'bc 0x{exit_at:x}');call('s');call('run 10000000')
            # Prove the resumed shell can read and execute a fresh command.
            for command in n.type_text('H:/AFTER.COM'):call(command)
            call('bp 0x4000');call('tap ENTER');call('run 2000000000')
            checks['shell accepts command after exit']=regs()['PC']==0x4000 and read(0x4000,5)==after_image
            call('bc 0x4000')
            return 'Snake native parity observations completed\n'

    after_image=bytes.fromhex('3e5ac30040')
    commands=['run 100000000']+n.type_text('H:/zvb/SNAKE.COM')+[
        'bp 0x4000','tap ENTER','run 2000000000','bc 0x4000','s','@parity','quit']
    def detach(command,response,directory):
        if command=='run 100000000':
            for path in (directory/'hostfs').rglob('*'):
                if path.is_file()and path.suffix.upper()in('.BIN','.ZPL','.ZPU','.PAS','.INC'):path.unlink()
    n.execute('snake-parity',commands,{'zvb/SNAKE.COM':image,'AFTER.COM':after_image},timeout=300,
              console_class=Snake,after_command=detach)
    failure_file=n.RUN/'snake-parity/input-error.json'
    failure=json.loads(failure_file.read_text())if failure_file.exists()else None
    report=dict(passed=bool(checks)and all(checks.values())and not failure,
        behavior_passed=bool(checks)and all(value for name,value in checks.items()
            if name!='wipe duration within two video frames of C')and not failure,checks=checks,
        observations=observations,observer_failure=failure,title_calls=len(title),
        saved_bytes=len(image),sha256=sha(image),emulator=str(n.EMULATOR),
        reference_sha256=reference['sha256'],capture=str(n.RUN))
    (n.RUN/'snake-parity-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    return 0 if report['passed']else 1


if __name__=='__main__':raise SystemExit(main())
