# ZVB: Zeal Video Board

**Development build: native Unit authoring and the saved Snake demo pass.
Full Snake timing parity and physical hardware verification remain open.**

`ZVB` gives native Pascal programs graphics, hardware sprites, CRC32, sound and
DMA through 45 routines. Its interface follows the public Zeal Video Board C SDK,
using Pascal names and records. The 41 published SDK functions have Pascal
counterparts; three macro equivalents and `GfxPaletteRead` bring the count to 45.
The public hardware headers also have named constants, documented in the
[complete register reference](zvb-registers.md). The implementation is Pascal, with short `Inline`
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
[units/zvb/ZVB-NOTICE.txt](../units/zvb/ZVB-NOTICE.txt). Demo assets have
separate credits in [CREDITS.md](../examples/zvb/CREDITS.md).

The authoritative interface is [ZVB.PAS](../units/zvb/ZVB.PAS).
`ZVBHW.INC` supplies public constants; `ZVBIO.INC`, `ZVBGFX.INC`, `ZVBPACK.INC` and
`ZVBDECODE.INC` beside it are implementation files.

## Direct register access

Use `Port[Name] := Value` for a write and `Value := Port[Name]` for a read.
Pascal assignment uses `:=`, and register constants name addresses rather than
mutable variables. All 64 public SDK register addresses and 69 object constants
are listed in [the register reference](zvb-registers.md). `_BIT` names are bit
positions; use `1 shl Name` to form a mask. Configuration/display controls are
non-banked; peripheral ports require `ZVB_CONFIG_DEV_IDX` selection. High-level
ZVB calls may change that selection. Raw access does not add validation or
automatic cleanup. SDK version constants describe the API reference, whereas
`ZVB_CONFIG_VER_*` ports report live firmware.

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

The complete public type set is:

| Type | Representation and use |
| --- | --- |
| `TGfxError`, `TDMAError` | Byte result codes, 0 success, 1 failure, 2 invalid argument |
| `TCRC32` | LongWord checksum, 4 bytes |
| `TPhysicalAddress` | LongWord physical address, 4 bytes; DMA uses its low 24 bits |
| `TGfxContext` | 5-byte library-managed record; fields listed under graphics setup |
| `TGfxTilesetOptions` | 5-byte record: Compression, FromByte, PaletteOffset, Opacity |
| `PGfxTilesetOptions` | 2-byte pointer to TGfxTilesetOptions; Nil selects default raw loading |
| `TGfxSprite` | 8-byte record; field layout listed under hardware sprites |
| `TSoundVoices` | Byte mask of tone voices and/or sample channel |
| `TSoundWaveform` | Byte waveform code including square-wave duty bits |
| `TSoundVolume` | Byte encoded volume, not a percentage |
| `TSoundSampleMode` | Byte sample-format code |
| `TSoundSamplesConfig` | 2-byte record: Mode, Divider |
| `TDMAFlags` | Byte packed descriptor flags |
| `TDMAClock` | Byte read/write clock fields; usable with the raw DMA clock port |
| `TDMADescriptor` | 12-byte hardware descriptor; field layout listed under DMA |
| `TDMADescriptorConfig` | 11-byte record: ReadAddress, WriteAddress, Length, Flags |

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
maps VRAM and stores one byte; sprite setters preserve the SDK's low/high
commit order. Tile and flag setters read and rewrite the companion byte, as
the SDK does. Palette, tilemap and sprite copies use Z80 LDIR.
Solid fills write each byte directly, matching SDK `memset_vram`; they never
read VRAM to propagate a fill value. Tileset copies use LDIR within a bank when
no pixel transformation is needed; transformed and cross-bank streams retain
bounded loops. CRC input uses
OTIR chunks. Sound routines select the device once per operation, and sample
playback configures once, polls the FIFO, streams bytes and drains it. DMA startup
converts the descriptor address, selects DMA, writes its three address bytes and
starts the transfer directly.

Device-memory reads remain intentional in `GfxPaletteRead` (the Pascal
palette-backup extension) and the SDK-style companion-byte preservation in
`GfxSpriteSetTile` and `GfxSpriteSetFlags`. The internal transfer leaf also
supports readback and companion preservation flags; current public loaders use
its copy, transform or fill paths. Packed loaders decode in RAM, then call
`GfxTilesetLoadNone`; they do not read VRAM for decompression.

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
write the eight scroll registers through `Port[]`. For example,
`Port[ZVB_CTRL_L0_SCR_Y_LOW] := 0;` writes the low byte; write
`ZVB_CTRL_L0_SCR_Y_HIGH` afterward to commit the Word. See the register
reference for all names, bank selection and bit positions.

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
attributes can select the wrong palette or upper tile bank.

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
  Combine square with `or` and a duty constant: `SoundDuty12`, `SoundDuty25`, `SoundDuty37`,
  `SoundDuty50`, `SoundDuty62`, `SoundDuty75` or `SoundDuty87`.
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

## Complete high-level constants

These are the values exported by the checked API. The separate
[hardware reference](zvb-registers.md) lists every raw SDK constant.

```pascal
const
  GfxSuccess = 0;
  GfxFailure = 1;
  GfxInvalidArg = 2;
  GfxSpritesCount = 128;
  SoundSampleTableSize = 256;
  DMADescLast = 1;
  GfxModeText640 = 0; GfxModeText320 = 1;
  GfxModeBitmap256 = 2; GfxModeBitmap320 = 3;
  GfxMode6408Bit = 4; GfxMode3208Bit = 5;
  GfxMode6404Bit = 6; GfxMode3204Bit = 7;
  TilesetCompNone = 0; TilesetComp1Bit = 1; TilesetComp4Bit = 2;
  TilesetComp2Bit = 3; TilesetCompRLE = 16; TilesetCompLZ = 17;
  SpriteNone = 0; SpriteTileBit9 = 1; SpriteBehindFG = 2;
  SpriteFlipY = 4; SpriteFlipX = 8;
  SpritePaletteShift = 4; SpritePaletteMask = 240;
  SpriteOptionNone = 0; SpriteOption32Px = 2;
  SoundVoice0 = 1; SoundVoice1 = 2; SoundVoice2 = 4; SoundVoice3 = 8;
  SoundSampleTable = 128;
  SoundSquare = 0; SoundTriangle = 1; SoundSawtooth = 2; SoundNoise = 3;
  SoundVol0 = 128; SoundVol25 = 0; SoundVol50 = 1;
  SoundVol75 = 2; SoundVol100 = 3;
  SoundDuty12 = 32; SoundDuty25 = 64; SoundDuty37 = 96;
  SoundDuty50 = 128; SoundDuty62 = 160; SoundDuty75 = 192;
  SoundDuty87 = 224;
  SoundSampleUInt8 = 0; SoundSampleUInt16 = 1; SoundSampleSInt16 = 2;
  DMAOpIncrement = 0; DMAOpDecrement = 1;
  DMAReadDecrement = 2; DMAWriteDecrement = 8;
  SpritePalette00 = 0;
  SpritePalette01 = 16;
  SpritePalette02 = 32;
  SpritePalette03 = 48;
  SpritePalette04 = 64;
  SpritePalette05 = 80;
  SpritePalette06 = 96;
  SpritePalette07 = 112;
  SpritePalette08 = 128;
  SpritePalette09 = 144;
  SpritePalette10 = 160;
  SpritePalette11 = 176;
  SpritePalette12 = 192;
  SpritePalette13 = 208;
  SpritePalette14 = 224;
  SpritePalette15 = 240;
```

## Complete typed routine declarations

These declarations match the public Unit interface. The sections above explain
buffer capacities, field ranges, results and side effects.

```pascal
{ Graphics: public zvb_gfx.h and RGB conversion macro. }
function RGB888ToRGB565(R, G, B: Byte): Word;
function GfxInitialize(Mode: Byte; var Context: TGfxContext): TGfxError;
procedure GfxEnableScreen(Enabled: Boolean);
function GfxPaletteLoad(var Context: TGfxContext; var Palette;
  Size: Word; FromColor: Byte): TGfxError;
{ Pascal extension, usable before GfxInitialize. }
function GfxPaletteRead(var Palette; Size: Word; FromColor: Byte): TGfxError;
function GfxTilesetLoad(var Context: TGfxContext; var Tileset;
  Size: Word; Options: PGfxTilesetOptions): TGfxError; { Nil = raw data. }
function GfxTilesetLoadNone(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte; Opacity: Boolean): TGfxError;
function GfxTilesetLoad1Bit(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte): TGfxError;
function GfxTilesetLoad2Bit(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte; Opacity: Boolean): TGfxError;
function GfxTilesetLoad4Bit(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte; Opacity: Boolean): TGfxError;
function GfxTilesetLoadRLE(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte; Opacity: Boolean): TGfxError;
function GfxTilesetLoadLZ(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte; Opacity: Boolean): TGfxError;
function GfxTilesetAddColorTile(var Context: TGfxContext;
  Index: Word; Color: Byte): TGfxError;
function GfxTilemapLoad(var Context: TGfxContext; var Tiles;
  Size, Layer, X, Y: Byte): TGfxError;
function GfxTilemapPlace(var Context: TGfxContext;
  Tile, Layer, X, Y: Byte): TGfxError;
function GfxWaitVBlank(var Context: TGfxContext): TGfxError;
function GfxWaitEndVBlank(var Context: TGfxContext): TGfxError;

{ Hardware sprites: public zvb_sprite.h. }
function GfxSpriteRender(var Context: TGfxContext; SpriteIndex: Byte;
  var Sprite: TGfxSprite): TGfxError;
function GfxSpriteRenderArray(var Context: TGfxContext; FromIndex: Byte;
  var Sprites; Count: Byte): TGfxError;
function GfxSpriteSetXY(var Context: TGfxContext; SpriteIndex: Byte;
  X, Y: Word): TGfxError;
function GfxSpriteSetX(var Context: TGfxContext;
  SpriteIndex: Byte; X: Word): TGfxError;
function GfxSpriteSetY(var Context: TGfxContext;
  SpriteIndex: Byte; Y: Word): TGfxError;
function GfxSpriteSetTile(var Context: TGfxContext;
  SpriteIndex, Tile: Byte): TGfxError;
function GfxSpriteSetFlags(var Context: TGfxContext;
  SpriteIndex, Flags: Byte): TGfxError;
function SpritePalette(Palette: Byte): Byte; { (Palette and 15) shl 4. }

{ Incremental hardware CRC32: public zvb_crc.h. }
procedure CRCInitialize(Reset: Boolean);
procedure CRCReset;
function CRCUpdate(var Buffer; Size: Word): TCRC32;

{ Sound: public zvb_sound.h and frequency conversion macro. }
function SoundFrequencyToDivider(Frequency: Word): Word;
procedure SoundInitialize(Reset: Boolean);
procedure SoundReset;
procedure SoundSetVoices(Voices: TSoundVoices; Divider: Word;
  Waveform: TSoundWaveform);
procedure SoundSetVoicesVol(Voices: TSoundVoices; Volume: TSoundVolume);
procedure SoundSetHold(Voices: TSoundVoices; Hold: Boolean);
procedure SoundSetChannels(LeftVoices, RightVoices: TSoundVoices);
procedure SoundSetVolume(Volume: TSoundVolume);
procedure SoundSetVolumes(Left, Right: TSoundVolume);
procedure SoundPlaySamples(var Config: TSoundSamplesConfig;
  var Samples; Length: Word);

{ DMA: public zvb_dma.h. Virtual conversion snapshots the current CPU mapping.
  Physically discontinuous buffers need separate descriptors; the SDK helper
  does not automatically split a transfer at CPU page boundaries. }
function DMAVirtToPhys(var Buffer): TPhysicalAddress;
function DMAPrepareDescriptor(var Descriptor: TDMADescriptor;
  var Config: TDMADescriptorConfig): TDMAError;
function DMASetRead(var Descriptor: TDMADescriptor;
  Address: TPhysicalAddress): TDMAError;
function DMASetReadVirt(var Descriptor: TDMADescriptor; var Buffer): TDMAError;
function DMASetWrite(var Descriptor: TDMADescriptor;
  Address: TPhysicalAddress): TDMAError;
function DMASetWriteVirt(var Descriptor: TDMADescriptor; var Buffer): TDMAError;
function DMAStartTransfer(var Descriptor: TDMADescriptor): TDMAError;
```

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

The default build validates and stages the native-authored stock `build/ZVB.ZPU`
(see [building](building.md) for re-authoring); selected fragments are
statically linked into the program. ZPU files are needed during compilation,
not execution. Runtime code, globals and program code receive final compact
addresses at link time; omitted Real code leaves no reserved hole. See the
[Unit memory guide](units.md#program-memory-layout) for placement and RAM
allocation, and [the demo guide](../examples/zvb/README.md) for binaries.

The private decoder stays in RAM and leaves the OS mapped. It validates each
token before writing, preserves 256 bytes of circular history across chunks,
checks source/history/destination bounds, then flushes through the ordinary VRAM
loader. Previous complete chunks may be visible on malformed input; the current
partial chunk is discarded. Regenerate its Inline stream with
`tools/generate-zvb-decoder.py`; `--check` detects drift. Internal words use
link-relocated Inline expressions, not fixed program addresses.

The generated stock ZVB source removes non-license comments, blank lines and
indentation while preserving quoted text and compiler directives. Readable
component files are unchanged; diagnostic positions refer to the exact
generated file compiled on Zeal. The result is guarded at 32,768 bytes to retain its two-source-
page authoring RAM budget. This is a qualification constraint for this stock
Unit, not the Pascal language's general source limit.

Snake's saved COM contains its code and assets and needs no PAS, INC or ZPU
files while running. Its collision wipe remains slower than the C reference;
full timing parity is not established. Use Com-file mode for the larger Snake
source: Memory compilation hits an asset-include allocation limit.

Physical display, controller and audio output have not been qualified on a
board. This API does not establish complete historical Turbo Pascal equivalence.
