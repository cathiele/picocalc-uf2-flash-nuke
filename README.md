# PicoCalc RP2350 flash delete uf2 variants for uf2 loader

Based on [pico-universal-flash-nuke](https://github.com/Gadgetoid/pico-universal-flash-nuke) the UF2 files in this repository delete different areas of the onboard Flash of the Pico 2 / Pico 2 W boards. 

These files can be used with a [PicoCalc](https://github.com/clockworkpi/PicoCalc) with [uf2loader](https://github.com/pelrun/uf2loader) installed.

It can be used to fully delete the flash of the Pico 2/2w except the uf2loader, or just delete the first half (2 MiB) of the flash, e.g. before reflashing an already set up [zeptoforth](https://github.com/tabemann/zeptoforth) uf2-image to prevent flash dict issues.

Pre-built UF2 files are available in the [latest release](releases/tag/latest):

- [`uf2loader-rp2350-full-nuke.uf2`](https://github.com/cathiele/picocalc-uf2-flash-nuke/releases/download/latest/uf2loader-rp2350-full-nuke.uf2) — erases the full flash (bootloader area preserved)
- [`uf2loader-rp2350-first-2MiB-nuke.uf2`](https://github.com/cathiele/picocalc-uf2-flash-nuke/releases/download/latest/uf2loader-rp2350-first-2MiB-nuke.uf2) — erases only the first 2 MiB of flash
- [`uf2loader-rp2350-2MiB64k-nuke-fat32.uf2`](https://github.com/cathiele/picocalc-uf2-flash-nuke/releases/download/latest/uf2loader-rp2350-2MiB64k-nuke-fat32.uf2) — erases from 2 MiB+64 KiB to end, then writes an empty zeptoforth blocks+FAT32 filesystem


For custom builds there are two configuration variables available.

## Build Configuration

Four CMake options control what gets erased and initialised. All are optional — without them the defaults apply (full flash, 8 KiB bootloader skipped, no FAT32 init).

| Option | Default | Description |
|---|---|---|
| `NUKE_BOOTLOADER_SIZE` | `0x2000` | Minimum flash offset protected from erasing (bootloader area). Set to `0` to allow erasing the bootloader region. |
| `NUKE_START_OFFSET` | `0` | Flash offset at which erasing **starts**. `0` means start right after the bootloader area. Must be ≥ `NUKE_BOOTLOADER_SIZE`; aligned down to sector boundary. |
| `NUKE_END_OFFSET` | `0` | Flash offset at which erasing **stops**. `0` means erase to end of flash. |
| `NUKE_FAT32_BLOCKS` | `0` | Set to `1` to write an empty zeptoforth-compatible blocks+FAT32 filesystem into the erased region after erasing. |

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
cmake -DPICO_BOARD=pico2 -DNUKE_END_OFFSET=0x200000 ..
make
```

**Pico 2 — erase only the second 2 MiB** (skip first 2 MiB, erase from 2 MiB to end):

```bash
mkdir build-rp2350 && cd build-rp2350
cmake -DPICO_BOARD=pico2 -DNUKE_START_OFFSET=0x200000 ..
make
```

**Pico 2 — erase from 2 MiB+64 KiB to end and init empty zeptoforth FAT32** (intended for PicoCalc with uf2loader):

```bash
mkdir build-rp2350 && cd build-rp2350
cmake -DPICO_BOARD=pico2 -DNUKE_START_OFFSET=0x210000 -DNUKE_FAT32_BLOCKS=1 ..
make
```

**Erase everything including bootloader area** (dangerous — use with care):

```bash
mkdir build-rp2350 && cd build-rp2350
cmake -DNUKE_BOOTLOADER_SIZE=0 ..
make
```
