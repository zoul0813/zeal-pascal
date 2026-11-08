# ZVB: Zeal Video Board

**Development build: native Unit authoring and the saved Snake demo pass.
Full Snake timing parity and physical hardware verification remain open.**

`ZVB` gives native Pascal programs graphics, hardware sprites, CRC32, sound and
DMA through 45 routines. Its interface follows the public Zeal Video Board C SDK,
using Pascal names and records. The implementation is Pascal, with short `Inline`
instruction sequences for atomic peripheral I/O, CRC feeding and temporary VRAM
mapping. It uses neither the C calling convention nor a C runtime.

`uses ZVB` alone changes no hardware state. Initialize each subsystem you use.

## A first program

This example draws an orange bar on dark blue and waits for a key.
`GfxInitialize` is only the start of a scene: this program also supplies the
palette, empties layer 1 and resets scrolling, because `GfxInitialize` does not.
`VIDEO.INC` from `examples/zvb` saves the screen state and restores it afterward;
ZVB itself has no cleanup routine.

```pascal
program FirstBar;
uses ZVB, Crt;
var Context: TGfxContext;
  Palette: array[0..1] of Word;
  Row: array[0..19] of Byte;
  I, Y: Integer; Error: TGfxError; Key: Char;
{$I VIDEO.INC}
begin
  VideoSaved := False;
  CrtInit;
  if SaveVideo then begin
    GfxEnableScreen(False);
    Error := GfxInitialize(GfxMode3208Bit, Context);
    Palette[0] := RGB888ToRGB565(0, 0, 64);
    Palette[1] := RGB888ToRGB565(255, 160, 0);
    if Error = GfxSuccess then
      Error := GfxPaletteLoad(Context, Palette, SizeOf(Palette), 0);
    if Error = GfxSuccess then
      Error := GfxTilesetAddColorTile(Context, 1, 1);
    for I := 0 to 127 do
      if Error = GfxSuccess then Error := GfxSpriteSetXY(Context, I, 0, 0);
    for I := 0 to 7 do Port[$94 + I] := 0;      { Neutral scrolling. }
    for I := 0 to 19 do Row[I] := 0;
    for Y := 0 to 14 do                         { Empty the visible layer 1. }
      if Error = GfxSuccess then
        Error := GfxTilemapLoad(Context, Row, 20, 1, 0, Y);
    for I := 0 to 19 do Row[I] := 1;
    if Error = GfxSuccess then
      Error := GfxTilemapLoad(Context, Row, 20, 0, 0, 7);
    if Error = GfxSuccess then begin
      GfxEnableScreen(True);
      if ParamCount = 0 then Key := ReadKey
    end
  end;
  RestoreVideo(Context);
  CrtExit
end.
```

The first example skips its key wait when any argument is supplied, allowing
automated execution. Its tested fixture expands the same `VIDEO.INC`.

## Installation and compilation

The stock files live beside `zpascal.zpl`, where the compiler finds units through
its existing search rules. The TF package installs these files in `T:/lib/`:

- `zpascal.zpl`
- `CRT.ZPU` and `CRT.PAS`
- `ZVB.ZPU`
- `ZVB.LIC` (Apache-2.0 license) and `ZVB.NTC` (SDK attribution and changes)

To save a demo executable in the IDE, save its Pascal source with **S**, select
**O → C (Com-file) → Q**, then press **C** to compile. This writes the COM beside
the source; run it from ZShell, or use **R** in Com-file mode. Memory mode
(**O → M → Q**) supports compilation and RAM execution without a disk workspace.
To publish a COM after Memory testing, select Com-file and compile again. The latest Snake source is qualified in Com-file mode; its Memory-mode
asset-include allocation exceeds available compiler RAM.

A saved program contains its reachable unit code and needs no ZPU or Pascal
source at run time. When redistributing a program containing ZVB code, include
`ZVB.LIC` and `ZVB.NTC` with it. Snake's embedded font also needs the credit and
license links in its demo credits; the game prints them on exit.

SDK attribution, license and Pascal changes are recorded in
[libraries/ZVB-NOTICE.txt](../libraries/ZVB-NOTICE.txt). Demo assets have
separate credits in [CREDITS.md](../examples/zvb/CREDITS.md).

The authoritative interface is [ZVB.PAS](../libraries/ZVB.PAS). `ZVBIO.INC`,
`ZVBGFX.INC` and `ZVBPACK.INC` beside it are private implementation files.

## Conventions

### What each number counts

Most mistakes with this unit are unit mistakes. `SizeOf(Buffer)` gives the right
value for every byte length, and the wrong value for a sprite `Count`.

| Value | Counts | Notes |
| --- | --- | --- |
| `Size`, `Length` | Bytes | Tileset and sample lengths count stored source bytes, not decoded pixels or samples |
| `FromByte` | Bytes | Offset into the 64 KB tileset: tile number × tile size |
| `FromColor` | Palette entries | Each entry occupies 2 bytes |
| Tile `Index`, `Tile` | Tiles | 8bpp tile = 256 bytes; 4bpp tile = 128 bytes |
| Tilemap `X`, `Y` | Tiles | Map is 80×40 |
| Sprite `X`, `Y` | Pixels | Include a 16-pixel bias |
| `SpriteIndex`, `FromIndex` | Sprite slots | 0..127 |
| `Count` | Sprite records | `GfxSpriteRenderArray` only; a record is 8 bytes |
| Tone `Divider` | Phase increment | Compute it with `SoundFrequencyToDivider`; sample `Divider` is different |
| `TPhysicalAddress` | Physical byte address | 24 bits; not a Pascal pointer or CPU address |

### Types

`Byte` is unsigned 8-bit, `Word` unsigned 16-bit and `LongWord` unsigned 32-bit.
Records are packed, with little-endian multi-byte fields. See
[Word and LongWord](unsigned-integers.md) for casts, arithmetic and initial limits.
Use `65535` or `Word($FFFF)` for an unsigned mask: a 16-bit hex literal keeps its
established signed Integer bit-pattern interpretation.

### Buffers

Input buffers are untyped `var` parameters because the language has no `const`
formal parameter syntax. Pass a variable, array element or typed constant, not
an expression. Input routines read these buffers without modifying them;
`GfxPaletteRead` writes into its destination buffer. The compiler cannot see an
untyped buffer's capacity, so the caller must supply at least the stated number of bytes.

Graphics buffers must lie outside CPU page 0 (`$0000..$3FFF`), which VRAM
temporarily replaces during a copy. Normal Pascal data and typed constants are
outside that page. Graphics routines return `GfxInvalidArg` for a page 0 buffer
or one that wraps past `$FFFF`. `SoundPlaySamples` silently refuses a wrapped
buffer. `CRCUpdate` does not check, so its caller must avoid a wrap.

### Results

Graphics and sprite functions return `TGfxError`: `GfxSuccess=0`, `GfxFailure=1`
or `GfxInvalidArg=2`. The current routines report every rejected call as
`GfxInvalidArg`; `GfxFailure` is kept for SDK compatibility. Check results before
continuing. Validation may be stricter than the C SDK. DMA functions return
`TDMAError`, with the same values. Sound and CRC setup routines are procedures,
and `CRCUpdate` returns the checksum, so these have no error result.

### Interrupts, mapping and shared hardware

Each routine restores the CPU memory mapping and the caller's interrupt-enable
state. VRAM is mapped only inside one bounded raw instruction sequence; Pascal
guards and OS calls run with the OS mapped.

Interrupts remain disabled for each VRAM copy, `CRCUpdate` and
`SoundPlaySamples` stream; a DMA transfer holds the bus. A long buffer therefore delays interrupts for
the length of that operation. Split long work into smaller calls when timing
matters; `CRC32.PAS` feeds 1 KB at a time.

Each peripheral operation selects its own bank, so CRC, sound and DMA calls can
interleave. They leave that peripheral selected, as the C API does. CRC and
sound are single shared devices, not per-context objects, so concurrent users
must coordinate.

ZVB saves nothing for you. A program that changes video or sound state owns
restoring it, and a run that ends without reaching that code leaves the state
behind. See [Graphics cleanup](../examples/zvb/README.md#graphics-cleanup).

### SDK implementation paths

The Pascal implementation follows the SDK's hardware processes: tile placement
maps VRAM and stores one byte; sprite setters write fields directly in the SDK's
low/high commit order. Palette, tilemap and sprite copies use Z80 LDIR.
Tileset copies and fills use LDIR within a bank when no pixel transformation is
needed; transformed and cross-bank streams retain bounded loops. CRC input uses
OTIR chunks. Sound routines select the device once per operation, and sample
playback configures once, polls the FIFO, streams bytes and drains it. DMA startup
converts the descriptor address, selects DMA, writes its three address bytes and
starts the transfer directly.

These are Pascal ABI instruction leaves, without SDCC or a C runtime. Pascal
argument validation and function frames still have their own cost. The Unit
preserves earlier hold bits when configuring voices, matching its documented
contract rather than the SDK implementation's unconditional selected-voice
release. Sample playback masks interrupts until the stream drains; use bounded
sample chunks when interrupt latency matters.

## Graphics setup and palette

| Routine | Effect |
| --- | --- |
| `GfxInitialize(Mode, Context)` | Fill Context and select Mode |
| `GfxEnableScreen(Enabled)` | Turn the display on or off |
| `RGB888ToRGB565(R, G, B)` | Return a Word color, truncating each component |
| `GfxPaletteLoad(Context, Palette, Size, FromColor)` | Copy RGB565 colors into the palette |
| `GfxPaletteRead(Palette, Size, FromColor)` | Snapshot RGB565 colors, including before graphics initialization |

The tile modes are `GfxMode6408Bit`, `GfxMode3208Bit`, `GfxMode6404Bit` and
`GfxMode3204Bit` (values 4..7). `GfxModeBitmap256` and `GfxModeBitmap320` (2..3)
follow the C interface and set `BitsPerPixel` to 4, as the SDK does. The 4bpp
loader and tile-index limits apply; this unit adds no bitmap drawing API.
`GfxModeText640` and `GfxModeText320` (0..1) are available for restoration, and
`GfxInitialize` rejects them.

`GfxInitialize` clears tile 0, the full 80×40 layer 0 map and palette entry 0.
It does **not** clear layer 1 or the sprites, reset scrolling, install a palette
or restore an earlier screen. Call `GfxEnableScreen(False)` first while preparing
a scene, then enable it explicitly. The unit has no scrolling routines; the demos
write the eight scroll registers `$94..$9B` directly.

Pass the same initialized `TGfxContext` to every Gfx call that takes a context. Its five one-byte
fields (`VideoMode`, `BitsPerPixel`, `AlphaIndex`, `VirtualPage`, `BackupPage`)
are library-managed.

`GfxPaletteLoad` takes a nonempty, even byte count of little-endian RGB565 colors.
The 512-byte palette holds 256 entries, and the load must fit from `FromColor`.
For an array of Words, pass `SizeOf(Palette)`.

`GfxPaletteRead` uses the same byte count and color bounds, writing into the
caller's buffer. It needs no context and changes no video mode. This Pascal
extension lets programs save the existing palette before initialization, without
pulling DMA support into the executable. Both operations preserve CPU mapping
and interrupt state.

## Tilesets and compression

The tileset holds 64 KB: 256 8bpp tiles or 512 4bpp tiles. `FromByte` is a byte
offset, not a tile number, and a load must stay inside the tileset. The supplied alphabet asset can use tile numbers
equal to character codes; `$4100` is tile 65
(`'A'`) × 256 bytes:

```pascal
Error := GfxTilesetLoadLZ(Context, Letters, SizeOf(Letters), $4100, 32, True);
```

| Format | `Compression` | Direct routine | 8bpp decoded bytes |
| --- | --- | --- | --- |
| Raw | `TilesetCompNone` = 0 | `GfxTilesetLoadNone` | Size |
| 1-bit | `TilesetComp1Bit` = 1 | `GfxTilesetLoad1Bit` | 8 × Size |
| 2-bit | `TilesetComp2Bit` = 3 | `GfxTilesetLoad2Bit` | 4 × Size |
| 4-bit | `TilesetComp4Bit` = 2 | `GfxTilesetLoad4Bit` | 2 × Size |
| RLE | `TilesetCompRLE` = 16 | `GfxTilesetLoadRLE` | Set by the stream |
| LZ | `TilesetCompLZ` = 17 | `GfxTilesetLoadLZ` | Set by the stream |

Direct routines take `(Context, Tileset, Size, FromByte, PaletteOffset, Opacity)`.
`GfxTilesetLoad1Bit` has no `Opacity` parameter.

- `Size` is always the **source** length in bytes.
- `PaletteOffset` is added to each pixel, modulo 256.
- `Opacity=True` keeps a zero source pixel at zero, rather than turning it into
  `PaletteOffset`. The 1-bit loader always applies the offset to zero pixels.
- The 1-bit and 2-bit loaders also support 4bpp output: they pack two pixels
  per byte, adding `PaletteOffset` modulo 16, and ignore `Opacity`, as the SDK
  does. Their output lengths are respectively 4 × Size and 2 × Size.
- The 4-bit, RLE and LZ loaders require 8bpp output. A Raw load in 4bpp copies
  packed bytes, so its offset and opacity test apply to each byte.
- Packed 1-, 2- and 4-bit data is decoded most-significant pixel first.
- Decoders hold a 256-byte chunk on the stack.

Use a direct routine when you know the format. `GfxTilesetLoad(Context, Tileset,
Size, Options)` dispatches at run time. A `Nil` Options pointer means raw data at
offset 0, palette offset 0, without opacity processing. `TGfxTilesetOptions`
contains `Compression: Byte`, `FromByte: Word`, `PaletteOffset: Byte` and
`Opacity: Boolean`. Pass the options record using the tested pointer syntax:

```pascal
Error := GfxTilesetLoad(Context, Data, 2, Ptr(Addr(Options)));
```

Set all four fields before the call, as shown in `tests/native/zvb/OPTIONS.PAS`.

`GfxTilesetAddColorTile(Context, Index, Color)` fills one tile. Index is 0..255
in 8bpp or 0..511 in 4bpp. In 4bpp, Color is the packed byte written to both
pixels, so repeat a nibble for a uniform color: `$33`, not `3`.

RLE and LZ streams follow the SDK's formats. LZ uses a 256-byte sliding history.

| Stream | First byte | Meaning |
| --- | --- | --- |
| RLE | `$00..$7F` | Next (low 7 bits + 1) bytes are literal |
| RLE | `$80..$FF` | Repeat the next byte (low 7 bits + 1) times |
| LZ | `$00..$3F` | This byte is one literal pixel |
| LZ | `$40..$7F` | Next (low 6 bits + 1) bytes are literal |
| LZ | `$80..$BF` | Copy (bits 4..5 + 3) pixels from (low 4 bits + 1) back |
| LZ | `$C0..$FF` | Copy (low 6 bits + 4) pixels; next byte + 1 is the distance back |

Truncated tokens, unavailable back references and destination overflow return
`GfxInvalidArg` without reading outside the supplied stream. A failed RLE or LZ
load does not roll back: earlier complete 256-byte chunks can remain in VRAM.

## Tilemaps

| Routine | Effect |
| --- | --- |
| `GfxTilemapLoad(Context, Tiles, Size, Layer, X, Y)` | Copy one row of `Size` tile numbers, 1..80 bytes |
| `GfxTilemapPlace(Context, Tile, Layer, X, Y)` | Set one map cell |

X is 0..79 and Y 0..39, and a row must fit before column 80. Layer is 0 or 1.
In 4bpp modes layer 0 stores the low tile index and layer 1 stores its
attribute byte: bit 0 is the ninth tile-index bit, bits 2/3 flip Y/X, and bits 4..7
select the palette. Clear layer 1 as well as layer 0 during 4bpp setup, or stale
attributes can select the wrong palette or upper tile bank. The fields follow
the [RTL attribute extraction](../reviews/zvb-rtl-reference.md).

In 8bpp, layer 1 draws over layer 0, so an empty tile there shows the background.
Tile 0 is empty after `GfxInitialize`. If you replace it, keep another empty tile
for layer 1, as Snake does with tile 15.

The map stays 80×40 whatever the mode. At zero scrolling, the 320×240 modes show
20×15 tiles and the 640×480 modes show 40×30.

## Frame timing

`GfxWaitVBlank(Context)` waits until vertical blank is active, and
`GfxWaitEndVBlank(Context)` waits until it ends. Pair them once per frame, so
that one blanking interval is not counted twice:

```pascal
while not Quit do begin
  Error := GfxWaitVBlank(Context);
  UpdateScreen;
  Error := GfxWaitEndVBlank(Context)
end;
```

Both block, and need a running ZVB display clock.

## Hardware sprites

There are 128 slots (`GfxSpritesCount`). A `TGfxSprite` is eight bytes:

| Offset | Field |
| --- | --- |
| 0 | `Y: Word` |
| 2 | `X: Word` |
| 4 | `Tile: Byte` |
| 5 | `Flags: Byte` |
| 6 | `Options: Word` |

| Routine | Effect |
| --- | --- |
| `GfxSpriteRender(Context, SpriteIndex, Sprite)` | Write one record |
| `GfxSpriteRenderArray(Context, FromIndex, Sprites, Count)` | Write `Count` contiguous records; `FromIndex + Count` must not exceed 128 |
| `GfxSpriteSetXY(Context, SpriteIndex, X, Y)` | Update X and Y |
| `GfxSpriteSetX(Context, SpriteIndex, X)` | Update X |
| `GfxSpriteSetY(Context, SpriteIndex, Y)` | Update Y |
| `GfxSpriteSetTile(Context, SpriteIndex, Tile)` | Update Tile |
| `GfxSpriteSetFlags(Context, SpriteIndex, Flags)` | Replace the whole Flags byte, including its palette nibble |
| `SpritePalette(N)` | Return palette bits `(N and 15) shl 4` |

Coordinates include the hardware's 16-pixel bias: X=16, Y=16 places the sprite at
the top-left edge. X=0 hides a 16-pixel-wide sprite beyond the left edge.
`GfxInitialize` leaves earlier sprites in place, so hide unused slots yourself:

```pascal
for I := 0 to 127 do Error := GfxSpriteSetXY(Context, I, 0, 0);
```

Combine Flags with `or`: `SpriteNone`, `SpriteTileBit9`, `SpriteBehindFG`,
`SpriteFlipX` and `SpriteFlipY`. In 4bpp, the upper nibble selects a palette.
`SpritePalette00` through `SpritePalette15` are the precomputed values, and
`SpritePaletteShift` and `SpritePaletteMask` describe the field. `SpritePalette`
corrects the SDK macro's palette-mask expression.

`Options` is `SpriteOptionNone` or `SpriteOption32Px`, which requests a 16×32
sprite using the adjacent tile.

## CRC32

| Routine | Effect |
| --- | --- |
| `CRCInitialize(True)`, `CRCReset` | Select the device and reset the accumulator |
| `CRCInitialize(False)` | Select the device without resetting |
| `CRCUpdate(Buffer, Size)` | Feed `Size` bytes and return the current `TCRC32` |

```pascal
CRCInitialize(True);
Value := CRCUpdate(Part1, SizeOf(Part1));
Value := CRCUpdate(Part2, SizeOf(Part2));   { Value covers both parts. }
```

Calls continue one hardware accumulator, and interleaved sound or DMA calls do
not reset it. A zero `Size` reads the current result without feeding data. The
check sequence `123456789` gives `$CBF43926`.

This is a single device: starting another calculation overwrites the previous
one. `Buffer` may be anywhere readable that does not wrap the CPU address space.
[`CRC32.PAS`](../examples/zvb/CRC32.PAS) checksums whole files and prints the
result as eight hexadecimal digits.

## Sound

There are four tone voices. `SoundVoice0`..`SoundVoice3` are the masks 1, 2, 4 and
8, and combine with `or`. `SoundSampleTable` (128) is the sample channel: it is
valid for channels and holds, not for voice frequency, waveform or volume.

To play a tone, set channels and master volume, configure a voice, then release
its hold. Hold it again to stop:

```pascal
SoundInitialize(True);
SoundSetChannels(SoundVoice0, SoundVoice0);
SoundSetVolume(SoundVol50);
SoundSetVoices(SoundVoice0, SoundFrequencyToDivider(440), SoundTriangle);
SoundSetHold(SoundVoice0, False);
{ ... }
SoundSetHold(SoundVoice0, True);
```

| Routine | Effect |
| --- | --- |
| `SoundInitialize(True)`, `SoundReset` | Hold all voices, set voice volumes to 100%, route the four voices to both channels and disable master output |
| `SoundInitialize(False)` | Only select the peripheral |
| `SoundFrequencyToDivider(Frequency)` | Convert Hz to a tone divider |
| `SoundSetVoices(Voices, Divider, Waveform)` | Configure voices, preserving the hold mask |
| `SoundSetVoicesVol(Voices, Volume)` | Set selected voice volumes |
| `SoundSetHold(Voices, Hold)` | True holds and mutes; False releases |
| `SoundSetChannels(LeftVoices, RightVoices)` | Route voice masks to each channel |
| `SoundSetVolume(Volume)` | Set both master channels |
| `SoundSetVolumes(Left, Right)` | Set master channels separately |
| `SoundPlaySamples(Config, Samples, Length)` | Play samples, blocking |

- **Waveforms:** `SoundSquare`, `SoundTriangle`, `SoundSawtooth`, `SoundNoise`.
  Combine square with `or` and a duty constant: `SoundDuty12`, `25`, `37`, `50`,
  `62`, `75` or `87`.
- **Volumes:** use `SoundVol0`, `SoundVol25`, `SoundVol50`, `SoundVol75` and
  `SoundVol100`. These are encodings (128, 0, 1, 2, 3), not percentages.
- **Tone divider:** the low 16 bits of `floor(65536*Hz/44091)`, using full
  unsigned arithmetic. Use 0..44090 Hz to avoid wrapping the Word result. The
  hardware's phase increment quantizes the frequency.

### Samples

```pascal
SoundInitialize(True);
SoundSetChannels(SoundSampleTable, SoundSampleTable);
SoundSetVolume(SoundVol50);
Config.Mode := SoundSampleUInt8;
Config.Divider := 7;                        { 44091/8: about 5511 Hz. }
SoundPlaySamples(Config, Samples, SizeOf(Samples));
SoundSetVolume(SoundVol0);
```

`TSoundSamplesConfig` is two bytes: `Mode`, then `Divider`. The sample rate is
44091/(Divider+1), a different formula from the tone divider. Modes are
`SoundSampleUInt8`, `SoundSampleUInt16` and `SoundSampleSInt16`; 16-bit samples
are little-endian. `Length` is in bytes, so 16-bit data needs an even length.
Divider 7 (about 5511 Hz) is the emulator-tested sample rate. Lower dividers feed
faster; their sustained rates are unmeasured and may underrun without an error.

`SoundPlaySamples` feeds the FIFO itself, so `Length` may exceed
`SoundSampleTableSize` (256), and it returns when the FIFO has drained. The prior
hold mask is restored. It has no result: a zero length, invalid mode, odd 16-bit
length or wrapped buffer silently plays nothing, as does a channel left unrouted
or a master volume left at zero.

## DMA

| Routine | Effect |
| --- | --- |
| `DMAVirtToPhys(Buffer)` | Return the physical address of `Buffer` under the current MMU mapping |
| `DMAPrepareDescriptor(Descriptor, Config)` | Fill a descriptor and zero its reserved bytes |
| `DMASetRead(Descriptor, Address)` | Set the physical source |
| `DMASetReadVirt(Descriptor, Buffer)` | Set the source from a variable |
| `DMASetWrite(Descriptor, Address)` | Set the physical destination |
| `DMASetWriteVirt(Descriptor, Buffer)` | Set the destination from a variable |
| `DMAStartTransfer(Descriptor)` | Run the transfer; the CPU resumes when it completes |

A copy within one 16 KB CPU page on each side looks like this:

```pascal
Config.ReadAddress := DMAVirtToPhys(Source);
Config.WriteAddress := DMAVirtToPhys(Destination);
Config.Length := SizeOf(Source);            { Bytes. }
Config.Flags := DMADescLast;
Error := DMAPrepareDescriptor(Descriptor, Config);
if Error = 0 then Error := DMAStartTransfer(Descriptor);
```

**Physical addresses.** `TPhysicalAddress` is a LongWord, and the descriptor
stores its low 24 bits. `DMAVirtToPhys` converts only the byte it is given,
as currently mapped. Neighboring virtual pages need not be neighboring physical
pages, so split a transfer at 16 KB CPU page boundaries, using separate
descriptors or calls. `ZVBCHECK.PAS` shows the page-splitting loop. The library
does not infer buffer sizes or validate physical memory ownership.

VRAM has fixed physical addresses. The focused `GFX.PAS` emulator client reads
palette, tilemaps, sprites and tiles through DMA at these addresses; this is not
a physical-board test:

| Area | Physical address |
| --- | --- |
| Layer 0 | `$100000` |
| Palette | `$100E00` |
| Layer 1 | `$101000` |
| Sprites | `$102800` |
| Tileset | `$110000` |

**Flags.** `DMADescLast` (1) marks a chain's final descriptor, so set it on every
single transfer. The read operation occupies bits 1..2 and the write operation
bits 3..4. `DMAOpIncrement` (0) and `DMAOpDecrement` (1) are the field values;
the ready-shifted `DMAReadDecrement` (2) and `DMAWriteDecrement` (8) can be
combined into Flags with `or`. A decrementing transfer starts at its last byte.
Leave reserved bits zero.

**Records.** `TDMADescriptorConfig` is 11 bytes: `ReadAddress`, `WriteAddress`
(LongWords), `Length` (Word bytes) and `Flags`. `TDMADescriptor` is 12 bytes:

| Offset | Field |
| --- | --- |
| 0 | `ReadAddressLow: Word` |
| 2 | `ReadAddressHigh: Byte` |
| 3 | `WriteAddressLow: Word` |
| 5 | `WriteAddressHigh: Byte` |
| 6 | `Length: Word` |
| 8 | `Flags: TDMAFlags` |
| 9 | `Reserved`, three bytes |

**Results.** `DMAStartTransfer` returns 2 if the 12-byte descriptor crosses a CPU
page boundary. The check covers only that descriptor, so keep a whole chain
within one page. `DMA.PAS` exercises a two-descriptor chain under emulation;
physical chained transfers are unqualified. Avoid zero-length chain entries on
unqualified hardware. Every other DMA helper returns 0. `TDMAClock` is declared for
the SDK's clock nibbles; no routine takes it yet.

## Demos, differences and verification

See [the demo guide](../examples/zvb/README.md) for Snake, sprites, the file
checksum utility and the CRC/DMA/sound check, and for what the graphics demos
save and restore.

Reference: [Zeal-VideoBoard-SDK](https://github.com/Zeal8bit/Zeal-VideoBoard-SDK),
commit `467e224aeaa40d0f45b61e495a5416b18c066ea7`, public API 1.0.0.0. Deliberate
Pascal differences:

- `GfxPaletteRead` for saving the palette before initialization;
- stronger bounds checks and malformed-stream rejection;
- a full layer 0 map clear in `GfxInitialize`;
- correct page-boundary copying;
- the corrected sprite palette helper;
- retained voice hold state in `SoundSetVoices`;
- full unsigned frequency arithmetic;
- compression decoders that preserve partial final chunks.

The Zeal-native compiler produces `build/ZVB.ZPU`; selected fragments are
statically linked into the program. ZPU files are needed during compilation,
not execution. Runtime code, globals and program code receive final compact
addresses at link time; omitted Real code leaves no reserved hole. See the
[runtime compaction notes](runtime-compaction-notes.md) for placement and RAM
allocation, and [the demo guide](../examples/zvb/README.md) for current binaries.

The current optimized Unit authoring run passes 374 native assertions and
produces a 55,529-byte ZPU containing 28,429 code bytes. Ten client regressions
pass 286 harness assertions, with sources/Units absent during saved execution;
the rebuilt hardware foundation separately passes 349 assertions. See the
[qualification evidence](zvb-optimized-evidence.json). CPU tests execute the actual Inline bytes extracted
from these sources, covering copies, fills, bank crossings, byte-write order,
CRC chunks, samples, DMA, mapping and interrupt state. They pass 966,798
assertions under host sanitizers. The direct tile placement leaf takes 338
T-states including the test's RET; a public Pascal call also incurs generated
frame, validation and return costs.

Snake's saved Com-file run completes 180 demo frames with all PAS/INC/ZPU files
removed. Current interactive behavior and pixels match the captured C reference;
full timing parity is still open: its 280-cell wipe takes 24,385,215 T-states
against C's 11,584,421, retaining the same per-cell placement plus 4 ms sleep.
The [parity evidence](snake-parity-evidence.json) deliberately reports this timing
failure. It does not certify complete gameplay performance equivalence.

The latest Snake source is qualified in Com-file mode. Memory compilation hits
an asset-include allocation limit and is not qualified. Physical display,
controller and audio output have not been qualified on a board. This API work
does not complete the historical Turbo Pascal specification.
