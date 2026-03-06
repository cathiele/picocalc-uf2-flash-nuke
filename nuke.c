/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Obliterate the contents of flash. This is a silly thing to do if you are
// trying to run this program from flash, so you should really load and run
// directly from SRAM. You can enable RAM-only builds for all targets by doing:
//
// cmake -DPICO_NO_FLASH=1 ..
//
// in your build directory. We've also forced no-flash builds for this app in
// particular by adding:
//
// pico_set_binary_type(flash_nuke no_flash)
//
// To the CMakeLists.txt app for this file. Just to be sure, we can check the
// define:
#if !PICO_NO_FLASH && !PICO_COPY_TO_RAM
#error "This example must be built to run from SRAM!"
#endif

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"

int main() {
    uint flash_size_bytes;

    uint8_t txbuf[4];
    uint8_t rxbuf[4];
    txbuf[0] = 0x9f;

    flash_do_cmd(txbuf, rxbuf, 4);

    flash_size_bytes = 1u << rxbuf[3];

    // Determine the end offset of the erase region.
    // NUKE_END_OFFSET == 0 means erase to end of flash.
    // Any other value stops erasing at that offset (aligned down to sector size), e.g. 0x200000 for 2 MiB.
    uint32_t erase_end = flash_size_bytes;
#if NUKE_END_OFFSET > 0
    if (erase_end > (uint32_t)(NUKE_END_OFFSET)) {
        // Round down to a 4096-byte sector boundary
        erase_end = (uint32_t)(NUKE_END_OFFSET) & ~(FLASH_SECTOR_SIZE - 1u);
    }
#endif

    // Skip the bootloader region at the beginning of flash.
    const uint32_t boot_size = (uint32_t)(NUKE_BOOTLOADER_SIZE) & ~(FLASH_SECTOR_SIZE - 1u);

    // Optionally start erasing at a higher offset (aligned down to sector size).
    // NUKE_START_OFFSET == 0 means start right after the bootloader area.
#if NUKE_START_OFFSET > 0
    const uint32_t start_offset = (uint32_t)(NUKE_START_OFFSET) & ~(FLASH_SECTOR_SIZE - 1u);
    const uint32_t erase_start = start_offset > boot_size ? start_offset : boot_size;
#else
    const uint32_t erase_start = boot_size;
#endif

    if (erase_end > erase_start) {
        flash_range_erase(erase_start, erase_end - erase_start);

#if NUKE_FAT32_BLOCKS
        extern void fat32_blocks_init(uint32_t erase_start, uint32_t erase_end);
        fat32_blocks_init(erase_start, erase_end);
#endif
    }

    watchdog_reboot(0, 0, 0);
}
