# The Minish Cap 3DS Platform

This target builds the native dual-screen Nintendo 3DS frontend.

Outside gameplay, the bottom screen shows a procedural gold-framed Triforce;
touching it opens the existing Minish Cap Settings hierarchy.

## Console Installation

Install the universal CIA:

```text
tmc-3ds-v1.3-E6.cia
```

Then create this directory on the SD card:

```text
sdmc:/3ds/The Minish Cap 3DS/
```

Place a legally obtained clean ROM there. Any `.gba` filename is accepted.
The port detects USA, European, and Japanese game codes at runtime and activates the
matching internal data profile before boot. Expected SHA-1 values:

```text
USA:    b4bd50e4131b027c334547b4524e2dbbd4227130
Europe: cff199b36ff173fb6faf152653d1bccf87c26fb7
JP:     6c5404a1effb17f481f352181d0f1c61a2765c5d
```

Two Chinese fan translations are accepted as experimental ROM variants, detected
at runtime by ROM fingerprint alongside the retail region:

```text
JP Chinese:  ba04cfbe93d12d2ad684c52234472fa12a5b53d7
EU Chinese:  9505d819eda125da13bc713334f17b5b9e8bc1ed
```

The BZMJ-based one rides the JP text pipeline; the BZMP-based one rides the EU
pipeline (extended character codes, patched font, and wider glyph-bank lookup).
Both are covered by a glyph-resolution audit that decodes every in-ROM message and
resolves each glyph to a valid font address.

The ROM is read locally from the SD card and is never copied into the CIA.

Audio requires a working 3DS DSP firmware setup. On Luma3DS, use Rosalina's
`Dump DSP firmware` option if homebrew audio is unavailable.

## Display

- Top screen: selectable Wide, Original, and Stretch aspect ratios.
- Display styles, in menu order: linearly filtered Blur, 2x sharp Bilinear,
  experimental 3x Ultra Sharp, and centered one-pixel-per-source-pixel Pixel
  Perfect. Fresh installations start with Bilinear and Stretch; valid saved
  choices are restored on later launches.
- Experimental Full View: on New 3DS, selecting Wide + Pixel Perfect renders
  compatible outdoor rooms at 400x240 and supported interiors at a centered
  200x120 logical view scaled exactly 2x. Dialogues, transitions and unsupported
  effects fall back safely to the established presentation.
- Bottom screen: 320x240 map, dungeon/status information and touch item UI.
- Rendering: PICA200/Citro2D presenter fed by the software GBA PPU.
- Performance profile: selected automatically from the detected console model.
- New 3DS: requests 804 MHz, L2 cache and access to the extra application core.
- New 3DS turbo: hold the C-stick in any direction and select 2x through 5x
  game speed from the Gameplay settings.
- Old 3DS: experimental CPU-renderer and audio optimizations plus adaptive
  presentation skipping target 60 Hz engine timing when visual rendering falls
  behind. Input, touch, audio and lifecycle processing continue on skipped
  presentation ticks; visual FPS may be lower.
- Bottom-screen scheduling: touch state is sampled on every engine tick;
  interactive hitboxes are promoted with the physically visible buffer, and
  unchanged static pages avoid redundant paint/upload work.
- Settings: Minish Cap-themed Screen, Gameplay, Developer, and Randomizer submenus with
  persistent options, a manual memory-dump command, and a live diagnostics
  overlay.
- Randomizer: persistent Project Picori randomizer mode in its own submenu. Mode
  changes require confirmation, clear only the active profile and related
  state, keep the ROM, and restart with isolated normal/randomized saves.
- Show FPS: measured presentation cadence in a compact lower-left top-screen
  box.
- Diagnostics: press `L + R + A` to pause the game, display `DUMP SAVED`, and
  create `dumps/dump-*` with top and bottom physical-framebuffer BMP and raw
  captures, EWRAM, IWRAM, VRAM, palettes, OAM, I/O and game state, frame
  visual and engine cadence, adaptive-skip data, per-core PPU timings, GPU
  work, audio buffer health, save
  persistence state, memory availability, lifecycle state and complete input
  data.
- System lifecycle: HOME, sleep and application close events are handled by the
  regular 3DS applet loop.
- Console: the development boot console remains visible during startup. Once
  gameplay begins, later stdout/stderr logs are detached from the bottom
  framebuffer so they cannot flicker over the map or touch interface.

The CIA metadata uses a stable title ID and requests SD card access for local
ROM and save data.

## Cheat codes

The port ships a built-in cheat menu with a set of codes already compiled in —
nothing needs to be installed. While playing, press `L + R + SELECT` to open
the menu on the bottom screen; the game pauses while it is open. Navigate with
`D-pad up/down`, toggle the highlighted cheat with `A`, and close with `B` (or
press `L + R + SELECT` again). Enabled cheats are applied continuously while on
and their on/off state persists in `tmc3ds.ini` (`cheat_enabled=...`).

The built-in list covers HP, max HP, rupees, bomb/arrow/shell counts, keys,
dungeon items, sword skills, scrolls, the full map, all figurines, and a few
item placeholders. To replace it with your own codes, add a text file named
`cheats.txt` in the app folder (the same folder as the ROM); when present it
replaces the built-in list:

```text
sdmc:/3ds/The Minish Cap 3DS/cheats.txt
```

The menu and cheat names render in the port's English text face; on a JP-ROM
build the decoded CJK font is disabled, so cheat names must be written in
ASCII. Non-ASCII lines are accepted by the parser but shown as blank names.

Addresses are the stock VBA/GameShark GBA codes (e.g. `02002AEA` = HP). The
port re-bases these onto the game's relocated state automatically — the
decomp builds the game as native code where the original EWRAM/IWRAM layout is
not preserved, so writes that fall inside a known game global (`gSave`,
`gPlayerState`, ...) are redirected to the matching port symbol; everything
else lands on the unused GBA-memory backing arrays and is harmless. No user
conversion is needed.

### File format

Each line is either a cheat name, a raw address/value pair, or a comment:

```text
# comment or blank lines are ignored

生命 02002AEA:A0          -> "name ADDR:VALUE": starts a cheat
02002AEB:A0              -> "ADDR:VALUE" alone: adds a write to the current cheat
02002E9D:63
钥匙数                   -> a lone name line is held and becomes the next
                             cheat's name if the very next line is a write;
                             otherwise it is discarded (used for value tables)

A键武器 02002AF4:xx       -> a non-hex value (xx) flags the cheat "NEED VALUE":
                             the line is kept but never written, and toggling
                             is skipped until you edit the value
```

The value's hex-digit count sets the write width: `2` digits = 1 byte, `4` = 2
bytes, `8` = 4 bytes. Addresses must sit in GBA memory (`0200xxxx` EWRAM,
`0300xxxx` IWRAM, ...). Up to 64 cheats, each with up to 16 write lines; names
are limited to 39 characters.

### The built-in list

This is exactly what ships compiled in; it is also a valid `cheats.txt`
override. The example file is `platform/3ds/cheats.example.txt`.

```text
# English sample (ASCII names for the JP-ROM renderer)
HP 02002AEA:A0
Max HP 02002AEB:A0
Rupees 02002B00:03E7
Rupees Max 02002AE8:03
All Sword Skills 0300402C:FFFF
All Scrolls 02002B44:FFFF
Bomb Count 02002AEC:63
Arrow Count 02002AED:63
Shell Count 02002B02:03E7
Key Count 02002E9D:63
Dungeon Items 02002E9E:6363
A-Button Item 02002AF4:xx
B-Button Item 02002AF5:xx
Sword Type 02002B32:xx
Partial Items (Careful) 02002B34:45545115
Body Size 03003FB0:xx
```

## Requirements

- devkitPro with devkitARM, libctru, Citro2D and Citro3D
- CMake with the devkitPro Nintendo 3DS toolchain
- `makerom` and `bannertool` for CIA packaging

Run:

```sh
chmod +x platform/3ds/build.sh
platform/3ds/build.sh
```

The universal packages are written under `build-3ds/game/`. No ROM, extracted
asset package or save data is included in either package.
