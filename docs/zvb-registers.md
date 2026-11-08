# ZVB hardware registers and constants

These compile-time constants are exported by `uses ZVB;`. Names match the SDK's
public `zvb_hardware.h` and `zvb_sdk.h`, with register identifiers capitalized.
Pascal is case-insensitive. No variable storage or routine is added to a saved
program merely by using a constant.

A constant names the port address; it is not itself a writable register variable.
Use `:=` to write and an expression to read:

```pascal
uses ZVB;
var Status: Byte;
begin
  Port[ZVB_CTRL_L0_SCR_Y_LOW] := 0;
  Port[ZVB_CTRL_L0_SCR_Y_HIGH] := 0;
  Status := Port[ZVB_CTRL_STATUS]
end.
```

## Non-banked and banked registers

Configuration ports `$80..$8F` and display controls `$90..$9D` are always
accessible. Peripheral ports `$A0..$AF` are shared: select their device through
`ZVB_CONFIG_DEV_IDX` before access. Their identical numeric addresses are intentional.
Text is bank 0, SPI 1, CRC 2, sound 3 and DMA 4; the `_IDX` constants give those values.
The high-level ZVB routines select their own bank and may leave another bank selected.
Raw Port operations do not restore the previous selection automatically:

```pascal
SavedBank := Port[ZVB_CONFIG_DEV_IDX];
Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_CRC_IDX;
LowCRC := Port[ZVB_PERI_CRC_BYTE0];
Port[ZVB_CONFIG_DEV_IDX] := SavedBank;
```

This sequence assumes no interrupt handler changes the peripheral selector.
Saving/restoring the bank does **not** make the intervening accesses atomic.
For multi-step banked operations, prefer the high-level ZVB routines: their
bounded Inline sequences preserve interrupt state and mask interrupts around
shared register accesses. If writing your own Inline sequence, save the incoming
interrupt-enable state, disable interrupts before saving/selecting the bank,
restore the bank, then restore that original interrupt state. Do not blindly
execute `EI`, and do not call the OS while interrupts are masked. Do not assume
all OS versions or application interrupt handlers leave the selector untouched.

This reads one byte of the current CRC without resetting it. In a complete
program, declare `SavedBank` and `LowCRC` as Byte. Coordinate access when another
routine or interrupt handler shares the same peripheral. SPI definitions expose
the TF-card controller, not a filesystem API; ordinary files still use Zeal OS.

Within sound bank 3, `$A0..$A2` also overlap: `ZVB_PERI_SOUND_SELECT` chooses
tone voices (bits 0..3) or the sample channel (`SoundSampleTable`, `$80`). Select
only `$80` before using `ZVB_PERI_SOUND_SAMPLE_FIFO`, `_DIV` and `_CONF`; selecting
a tone voice addresses its frequency/waveform registers instead.

## Bits, latches and physical addresses

`_BIT` constants are bit positions, not masks. The C `BIT(N)` equivalent is
`1 shl N`; the C peripheral-map macro is the selector assignment above:

```pascal
InVBlank := (Port[ZVB_CTRL_STATUS] and
  (1 shl ZVB_CTRL_STATUS_VBLANK_BIT)) <> 0;
```

Declare `InVBlank` as Boolean. Read 16-bit raster positions low byte first;
that read latches the high byte. Write 16-bit scrolling and sound frequencies
low byte first, then high: the high-byte write commits the pair. For example:

```pascal
Port[ZVB_CTRL_L0_SCR_Y_LOW] := Byte(ScrollY);
Port[ZVB_CTRL_L0_SCR_Y_HIGH] := Byte(ScrollY shr 8);
```

`ScrollY` is a Word. Port access does not validate read-only/write-only permissions,
reserved bits, video mode or field ranges. Follow the hardware contract for each
register. The SDK reference linked below describes those permissions; constants
provide addresses, not a new checked access layer.

`VID_MEM_*_ADDR` and `VID_MEM_PHYS_ADDR_START` are physical addresses for DMA,
not CPU pointers for `Mem[]`. Values above 65535 are LongWord constants.
`VID_MEM_*_OFFSET` are byte offsets from the physical video-memory base.
`ZVB_SDK_*` describes this API reference version; read `ZVB_CONFIG_VER_*` ports
for the running board's firmware version.

## Register address reference

### Configuration

| Constant | Port |
| --- | --- |
| `ZVB_CONFIG_VER_REV` | `$80` |
| `ZVB_CONFIG_VER_MIN` | `$81` |
| `ZVB_CONFIG_VER_MAJ` | `$82` |
| `ZVB_CONFIG_DEV_IDX` | `$8E` |
| `ZVB_CONFIG_PHYS_ADDR` | `$8F` |

### Display controls

| Constant | Port |
| --- | --- |
| `ZVB_CTRL_VPOS_LOW` | `$90` |
| `ZVB_CTRL_VPOS_HIGH` | `$91` |
| `ZVB_CTRL_HPOS_LOW` | `$92` |
| `ZVB_CTRL_HPOS_HIGH` | `$93` |
| `ZVB_CTRL_L0_SCR_Y_LOW` | `$94` |
| `ZVB_CTRL_L0_SCR_Y_HIGH` | `$95` |
| `ZVB_CTRL_L0_SCR_X_LOW` | `$96` |
| `ZVB_CTRL_L0_SCR_X_HIGH` | `$97` |
| `ZVB_CTRL_L1_SCR_Y_LOW` | `$98` |
| `ZVB_CTRL_L1_SCR_Y_HIGH` | `$99` |
| `ZVB_CTRL_L1_SCR_X_LOW` | `$9A` |
| `ZVB_CTRL_L1_SCR_X_HIGH` | `$9B` |
| `ZVB_CTRL_VIDEO_MODE` | `$9C` |
| `ZVB_CTRL_STATUS` | `$9D` |

### Text peripheral

| Constant | Port |
| --- | --- |
| `ZVB_PERI_TEXT_PRINT_CHAR` | `$A0` |
| `ZVB_PERI_TEXT_CURS_Y` | `$A1` |
| `ZVB_PERI_TEXT_CURS_X` | `$A2` |
| `ZVB_PERI_TEXT_SCROLL_Y` | `$A3` |
| `ZVB_PERI_TEXT_SCROLL_X` | `$A4` |
| `ZVB_PERI_TEXT_COLOR` | `$A5` |
| `ZVB_PERI_TEXT_CURS_TIME` | `$A6` |
| `ZVB_PERI_TEXT_CURS_CHAR` | `$A7` |
| `ZVB_PERI_TEXT_CURS_COLOR` | `$A8` |
| `ZVB_PERI_TEXT_CTRL` | `$A9` |

### SPI peripheral

| Constant | Port |
| --- | --- |
| `ZVB_PERI_SPI_CTRL` | `$A1` |
| `ZVB_PERI_SPI_CLK_DIV` | `$A2` |
| `ZVB_PERI_SPI_RAM_LEN` | `$A3` |
| `ZVB_PERI_SPI_FIFO` | `$A7` |
| `ZVB_PERI_SPI_ARRAY_0` | `$A8` |
| `ZVB_PERI_SPI_ARRAY_1` | `$A9` |
| `ZVB_PERI_SPI_ARRAY_2` | `$AA` |
| `ZVB_PERI_SPI_ARRAY_3` | `$AB` |
| `ZVB_PERI_SPI_ARRAY_4` | `$AC` |
| `ZVB_PERI_SPI_ARRAY_5` | `$AD` |
| `ZVB_PERI_SPI_ARRAY_6` | `$AE` |
| `ZVB_PERI_SPI_ARRAY_7` | `$AF` |

### CRC peripheral

| Constant | Port |
| --- | --- |
| `ZVB_PERI_CRC_CTRL` | `$A0` |
| `ZVB_PERI_CRC_DATA_IN` | `$A1` |
| `ZVB_PERI_CRC_BYTE0` | `$A4` |
| `ZVB_PERI_CRC_BYTE1` | `$A5` |
| `ZVB_PERI_CRC_BYTE2` | `$A6` |
| `ZVB_PERI_CRC_BYTE3` | `$A7` |

### Sound peripheral

| Constant | Port |
| --- | --- |
| `ZVB_PERI_SOUND_FREQ_LOW` | `$A0` |
| `ZVB_PERI_SOUND_FREQ_HIGH` | `$A1` |
| `ZVB_PERI_SOUND_WAVE` | `$A2` |
| `ZVB_PERI_SOUND_VOLUME` | `$A3` |
| `ZVB_PERI_SOUND_SAMPLE_FIFO` | `$A0` |
| `ZVB_PERI_SOUND_SAMPLE_DIV` | `$A1` |
| `ZVB_PERI_SOUND_SAMPLE_CONF` | `$A2` |
| `ZVB_PERI_SOUND_LEFT_CHANNEL` | `$AB` |
| `ZVB_PERI_SOUND_RIGHT_CHANNEL` | `$AC` |
| `ZVB_PERI_SOUND_HOLD` | `$AD` |
| `ZVB_PERI_SOUND_MASTER_VOL` | `$AE` |
| `ZVB_PERI_SOUND_SELECT` | `$AF` |

### DMA peripheral

| Constant | Port |
| --- | --- |
| `ZVB_PERI_DMA_CTRL` | `$A0` |
| `ZVB_PERI_DMA_ADDR0` | `$A1` |
| `ZVB_PERI_DMA_ADDR1` | `$A2` |
| `ZVB_PERI_DMA_ADDR2` | `$A3` |
| `ZVB_PERI_DMA_CLK_DIV` | `$A9` |

## Other SDK constants

Names ending `_BIT` are indices. Other values below are mode codes, masks,
peripheral indices, sizes, physical addresses or offsets as named.
`ZVB_PERI_DMA_CLK_DIV` accepts the SDK clock byte: low four bits are the read
cycle field and high four the write cycle field (`TDMAClock`); no high-level
routine currently sets that port.

| Constant | Value |
| --- | --- |
| `VID_MEM_PHYS_ADDR_START` | `$100000` |
| `VID_MEM_LAYER0_OFFSET` | `0` |
| `VID_MEM_PALETTE_OFFSET` | `$E00` |
| `VID_MEM_LAYER1_OFFSET` | `$1000` |
| `VID_MEM_SPRITE_OFFSET` | `$2800` |
| `VID_MEM_FONT_OFFSET` | `$3000` |
| `VID_MEM_TILESET_OFFSET` | `$10000` |
| `VID_MEM_LAYER0_ADDR` | `$100000` |
| `VID_MEM_PALETTE_ADDR` | `$100E00` |
| `VID_MEM_LAYER1_ADDR` | `$101000` |
| `VID_MEM_SPRITE_ADDR` | `$102800` |
| `VID_MEM_FONT_ADDR` | `$103000` |
| `VID_MEM_TILESET_ADDR` | `$110000` |
| `ZVB_CONFIG_BASE` | `$80` |
| `ZVB_CTRL_BASE` | `$90` |
| `ZVB_CTRL_VID_MODE_TEXT_640` | `0` |
| `ZVB_CTRL_VID_MODE_TEXT_320` | `1` |
| `ZVB_CTRL_VID_MODE_BITMAP_256_MODE` | `2` |
| `ZVB_CTRL_VID_MODE_BITMAP_320_MODE` | `3` |
| `ZVB_CTRL_VID_MODE_GFX_640_8BIT` | `4` |
| `ZVB_CTRL_VID_MODE_GFX_320_8BIT` | `5` |
| `ZVB_CTRL_VID_MODE_GFX_640_4BIT` | `6` |
| `ZVB_CTRL_VID_MODE_GFX_320_4BIT` | `7` |
| `ZVB_CTRL_VID_MODE_GFX_LAST` | `7` |
| `ZVB_CTRL_STATUS_HBLANK_BIT` | `0` |
| `ZVB_CTRL_STATUS_VBLANK_BIT` | `1` |
| `ZVB_CTRL_STATUS_SCREEN_ON_BIT` | `7` |
| `ZVB_PERI_BASE` | `$A0` |
| `ZVB_PERI_TEXT_IDX` | `0` |
| `ZVB_PERI_TEXT_CTRL_SAVE_CURSOR_BIT` | `7` |
| `ZVB_PERI_TEXT_CTRL_RESTORE_CURSOR_BIT` | `6` |
| `ZVB_PERI_TEXT_CTRL_AUTO_SCROLL_X_BIT` | `5` |
| `ZVB_PERI_TEXT_CTRL_AUTO_SCROLL_Y_BIT` | `4` |
| `ZVB_PERI_TEXT_CTRL_WAIT_ON_WRAP_BIT` | `3` |
| `ZVB_PERI_TEXT_CTRL_SCROLL_Y_OCCUR_BIT` | `0` |
| `ZVB_PERI_TEXT_CTRL_NEXTLINE` | `0` |
| `ZVB_PERI_SPI_IDX` | `1` |
| `ZVB_PERI_SPI_CTRL_START_BIT` | `7` |
| `ZVB_PERI_SPI_CTRL_RESET_BIT` | `6` |
| `ZVB_PERI_SPI_CTRL_CS_START_BIT` | `5` |
| `ZVB_PERI_SPI_CTRL_CS_STOP_BIT` | `4` |
| `ZVB_PERI_SPI_CTRL_CS_BIT` | `3` |
| `ZVB_PERI_SPI_CTRL_IDLE_BIT` | `0` |
| `ZVB_PERI_SPI_ARRAY_LEN` | `8` |
| `ZVB_PERI_CRC_IDX` | `2` |
| `IO_CRC32_CTRL_RESET_BIT` | `0` |
| `ZVB_PERI_SOUND_IDX` | `3` |
| `ZVB_PERI_SOUND_SQUARE` | `0` |
| `ZVB_PERI_SOUND_TRIANGLE` | `1` |
| `ZVB_PERI_SOUND_SAWTOOTH` | `2` |
| `ZVB_PERI_SOUND_NOISE` | `3` |
| `ZVB_SAMPLE_CONF_SIZE_BIT` | `0` |
| `ZVB_SAMPLE_CONF_RSVD_BIT` | `1` |
| `ZVB_SAMPLE_CONF_SIGN_BIT` | `2` |
| `ZVB_SAMPLE_CONF_FULL_BIT` | `6` |
| `ZVB_SAMPLE_CONF_READY_BIT` | `7` |
| `ZVB_PERI_SOUND_VOL_DISABLE` | `$80` |
| `ZVB_PERI_SOUND_VOL_25` | `0` |
| `ZVB_PERI_SOUND_VOL_50` | `1` |
| `ZVB_PERI_SOUND_VOL_75` | `2` |
| `ZVB_PERI_SOUND_VOL_100` | `3` |
| `ZVB_PERI_DMA_IDX` | `4` |
| `ZVB_PERI_DMA_CTRL_START` | `$80` |
| `ZVB_PERI_DMA_OP_INC` | `0` |
| `ZVB_PERI_DMA_OP_DEC` | `1` |
| `ZVB_SDK_MAJOR` | `1` |
| `ZVB_SDK_MINOR` | `0` |
| `ZVB_SDK_REV` | `0` |
| `ZVB_SDK_VER` | `0` |

## Reference and maintenance

See [the Pascal ZVB guide](zvb-library.md) for the checked high-level API,
buffers, errors, units, examples and supported behavior. Raw register constants
also expose text and SPI hardware for which ZVB has no new high-level wrappers.

Source: [SDK public hardware header](https://github.com/Zeal8bit/Zeal-VideoBoard-SDK/blob/467e224aeaa40d0f45b61e495a5416b18c066ea7/include/zvb_hardware.h),
SDK 1.0.0.0. Its register comments specify raster latching (lines 72–78),
scroll R/W and high-byte commit (80–98), and sound-frequency commit (262–266).
Regenerate definitions, value checks and this reference with:

```sh
python3 tools/generate-zvb-hardware.py --sdk /path/to/Zeal-VideoBoard-SDK
```

Add `--check` to verify generated files without changing them.
