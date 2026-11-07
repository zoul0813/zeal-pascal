{ SPDX-FileCopyrightText: 2024-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
  SPDX-License-Identifier: Apache-2.0
  Pascal rewrite; see ZVB-NOTICE.txt for source and changes. }
{ Zeal Video Board unit: Pascal implementation using the public C API as reference. }
unit ZVB;
interface

type
  TGfxError = Byte;
  TDMAError = Byte;
  TCRC32 = LongWord;
  TPhysicalAddress = LongWord; { DMA stores the low24 bits, as in the C API. }

  TGfxContext = record
    VideoMode, BitsPerPixel, AlphaIndex: Byte;
    VirtualPage, BackupPage: Byte; { Managed by the library. }
  end;

  TGfxTilesetOptions = record
    Compression: Byte;
    FromByte: Word;
    PaletteOffset: Byte;
    Opacity: Boolean;
  end;
  PGfxTilesetOptions = ^TGfxTilesetOptions;

  TGfxSprite = record
    Y, X: Word; { SDK coordinates:16 places the sprite at the screen edge. }
    Tile, Flags: Byte;
    Options: Word;
  end;

  TSoundVoices = Byte; { Voice masks may be combined with Or. }
  TSoundWaveform = Byte; { Includes the SDK square-wave duty-cycle bits. }
  TSoundVolume = Byte; { SDK encoding, not an arbitrary percentage. }
  TSoundSampleMode = Byte;
  TSoundSamplesConfig = record
    Mode: TSoundSampleMode;
    Divider: Byte;
  end;

  TDMAFlags = Byte; { Packed SDK Last/ReadOperation/WriteOperation bits. }
  TDMAClock = Byte; { Packed read/write cycle nibbles. }
  TDMADescriptor = record
    ReadAddressLow: Word;
    ReadAddressHigh: Byte;
    WriteAddressLow: Word;
    WriteAddressHigh: Byte;
    Length: Word;
    Flags: TDMAFlags;
    Reserved: array[0..2] of Byte;
  end;
  TDMADescriptorConfig = record
    ReadAddress, WriteAddress: TPhysicalAddress;
    Length: Word;
    Flags: TDMAFlags;
  end;

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

implementation
{$I ZVBIO.INC}
{$I ZVBGFX.INC}
end.
