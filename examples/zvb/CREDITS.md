# ZVB example credits

## Snake assets

Snake game assets by **Clear_code**, CC0 1.0:
https://opengameart.org/content/snake-game-assets
https://creativecommons.org/publicdomain/zero/1.0/

Pixel Block Font by **JROB774**, CC BY 3.0:
https://opengameart.org/content/pixel-block-font
https://creativecommons.org/licenses/by/3.0/

**Changes:** the Zeal 8-bit Computer project adapted these assets for 16×16 tiles
and the ZVB palette. Snake embeds the SDK's snake tiles, numerals, selected original title/score glyphs and palettes.
The original six-sprite title and SCORE label use the original ASCII tile IDs.
SNAKEFONT.INC retains A,C,E,K,N,O,R,S; other slots are transparent.
The font pixels are expanded from 4bpp to 8bpp, then all tiles are compressed
individually with the SDK LZ encoder. The Pascal include files embed those
compressed streams. Independent decoding verifies unchanged pixel values.

**Redistribution:** the font credit above must accompany redistributed copies of
the demo and its font assets. That includes a saved Snake executable, which
embeds the font.

Source: Zeal-VideoBoard-SDK, commit
`467e224aeaa40d0f45b61e495a5416b18c066ea7`, `examples/snake/`.
`assets.json` records the hashes and sizes of the SDK asset files, and the hash of
the palette source, plus compressed and decoded payload hashes and codec hashes.

## Project code

The Pascal Snake implementation is new project code following the SDK demo (CC0-1.0).
`CRC32.PAS` adapts the intent of the SDK's `examples/crc32` (CC0-1.0) to Pascal,
`ParamStr` and three local syscall wrappers for raw bytes.
