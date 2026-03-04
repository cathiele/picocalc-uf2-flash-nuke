# PicoCalc RP2350 flash delete uf2 variants for uf2 loader

Based on [pico-universal-flash-nuke](https://github.com/Gadgetoid/pico-universal-flash-nuke) the UF2 files in this repository delete different areas of the onboard Flash of the Pico 2 / Pico 2 W boards. 

These files can be used with a [PicoCalc](https://github.com/clockworkpi/PicoCalc) with [uf2loader](https://github.com/pelrun/uf2loader) installed.

It can be used to fully delete the flash of the Pico 2/2w except the uf2loader, or just delete the first half (2 MiB) of the flash, e.g. before reflashing an already set up [zeptoforth](https://github.com/tabemann/zeptoforth) uf2-image to prevent flash dict issues.

Pre-built UF2 files are available in the [latest release](releases/tag/latest):

- [`uf2loader-rp2350-full-nuke.uf2`](releases/download/latest/uf2loader-rp2350-full-nuke.uf2) — erases the full flash (bootloader area preserved)
- [`uf2loader-rp2350-first-2MiB-nuke.uf2`](releases/download/latest/uf2loader-rp2350-first-2MiB-nuke.uf2) — erases only the first 2 MiB of flash


For custom builds there are two configuration variables available.

## Build Configuration

Two CMake options control what gets erased. All are optional — without them the defaults apply (full flash, 8 KiB bootloader skipped).

| Option | Default | Description |
|---|---|---|
| `NUKE_BOOTLOADER_SIZE` | `0x2000` | Flash offset to **start** erasing from (bootloader area that is kept). Set to `0` to erase everything including the bootloader region. |
| `NUKE_MAX_BYTES` | `0` | Flash offset at which erasing **stops**. `0` means erase to end of flash. |

### Examples

**Pico 2 — erase full flash** (4 MiB, bootloader protected, RP2350):

```bash
mkdir build-rp2350 && cd build-rp2350
cmake -DPICO_BOARD=pico2 ..
make
```

**Pico 2 — erase only first 2 MiB** (e.g. to preserve the upper half):

```bash
mkdir build-rp2350 && cd build-rp2350
cmake -DPICO_BOARD=pico2 -DNUKE_MAX_BYTES=0x200000 ..
make
```

**Erase everything including bootloader area** (dangerous — use with care):

```bash
mkdir build-rp2350 && cd build-rp2350
cmake -DNUKE_BOOTLOADER_SIZE=0 ..
make
```
