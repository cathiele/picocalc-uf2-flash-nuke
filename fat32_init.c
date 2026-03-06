/**
 * fat32_init.c — write a zeptoforth-compatible blocks+FAT32 filesystem to flash.
 *
 * The zeptoforth blocks layer maps flash like this:
 *   Each 64 KiB flash "sector":
 *     bytes    0–1023  : metadata (block-ID map + old-flag map + erase count)
 *     bytes 1024–2047  : slot 0 data  (1 KiB = 2 × 512-byte FAT32 sectors)
 *     bytes 2048–3071  : slot 1 data
 *     ...
 *     bytes 64512–65535: slot 62 data
 *
 * A "block ID" is the logical FAT32 block number (each block holds 2 FAT32
 * sectors).  Block IDs are stored in the metadata so the wear-levelling layer
 * can find them.
 *
 * For an empty filesystem we only need to write three non-trivial flash blocks
 * into the first 64 KiB sector of the region:
 *
 *   slot 0  (block ID 0)  →  MBR  (512 B) + FAT32 VBR  (512 B)
 *   slot 1  (block ID 1)  →  FSInfo (512 B) + FAT1[0]   (512 B)
 *   slot 2  (block ID 5)  →  FAT1[7] zeros + FAT2[0]   (512 B)
 *
 * All other flash blocks default to zeros when the blocks layer reads them
 * (unwritten slots return zeros), which is correct for empty FAT and root-dir.
 */

#include "hardware/flash.h"
#include <string.h>
#include <stdint.h>

/* zeptoforth blocks-layer constants */
#define BLK_SECTOR_SZ   65536u   /* 64 KiB: one flash "sector"              */
#define BLK_BLOCK_SZ    1024u    /* 1 KiB:  one flash block (2 FAT32 secs)  */
#define BLK_SLOTS       63u      /* data slots per flash sector              */

/* Byte offsets within the 1-KiB sector metadata block */
#define META_ID_OFF     0u       /* block-ID map:  BLK_SLOTS × 4 bytes      */
#define META_OLD_OFF    252u     /* old-flag map:  BLK_SLOTS × 4 bytes      */
#define META_ERASE_OFF  504u     /* erase count:   4 bytes                  */

#define FAT_SECTOR_SZ   512u

/* 1 KiB scratch buffer — must live in RAM for flash_range_program */
static uint8_t fat32_buf[BLK_BLOCK_SZ];

static inline void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
}

static inline void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void write_slot(uint32_t sector_base, uint32_t slot) {
    /* Slot N data lives at sector_base + (N+1) * BLK_BLOCK_SZ */
    flash_range_program(sector_base + (slot + 1u) * BLK_BLOCK_SZ,
                        fat32_buf, BLK_BLOCK_SZ);
}

/**
 * Initialise a zeptoforth blocks+FAT32 filesystem in the flash region
 * [erase_start, erase_end).  The region must already have been erased.
 *
 * erase_start / erase_end are flash offsets (from 0, not XIP base).
 *
 * The function aligns both ends to 64 KiB boundaries before computing the
 * filesystem size.
 */
void fat32_blocks_init(uint32_t erase_start, uint32_t erase_end) {
    /* Align to 64 KiB sector boundaries */
    uint32_t fs_start = (erase_start + BLK_SECTOR_SZ - 1u) & ~(BLK_SECTOR_SZ - 1u);
    uint32_t fs_end   = erase_end & ~(BLK_SECTOR_SZ - 1u);
    if (fs_end <= fs_start) return;

    /* Number of 64 KiB sectors available */
    uint32_t n_sectors  = (fs_end - fs_start) / BLK_SECTOR_SZ;

    /* Total FAT32 block-device sectors: each slot holds 2 FAT32 sectors */
    uint32_t total_bd   = n_sectors * BLK_SLOTS * 2u;
    /* Partition occupies all block-device sectors except the first (MBR) */
    uint32_t total_part = total_bd - 1u;

    /* FAT32 geometry */
    const uint32_t NRESV = 2u;   /* reserved sectors: VBR + FSInfo */
    const uint32_t NFATS = 2u;
    const uint32_t SPC   = 4u;   /* sectors per cluster             */

    /* Converge on the FAT size in sectors */
    uint32_t fat_sz = 8u;
    for (int i = 0; i < 5; i++) {
        uint32_t data_s   = total_part - NRESV - NFATS * fat_sz;
        uint32_t clusters = data_s / SPC;
        fat_sz = ((clusters + 2u) * 4u + FAT_SECTOR_SZ - 1u) / FAT_SECTOR_SZ;
    }

    uint32_t data_secs      = total_part - NRESV - NFATS * fat_sz;
    uint32_t total_clusters = data_secs / SPC;
    uint32_t free_clusters  = total_clusters - 1u;  /* cluster 2 = root dir */

    /* ── Sector metadata (first 1 KiB of fs_start) ──────────────────────── */
    /*
     * Slot assignments:
     *   slot 0  → block ID 0  (MBR + VBR)
     *   slot 1  → block ID 1  (FSInfo + FAT1[0])
     *   slot 2  → block ID 5  (FAT1[7] zeros + FAT2[0])
     *
     * Old-flag map: all 0xFF = "current" — the erased state, nothing to write.
     * Erase count:  1 (the nuke step erased this sector once).
     */
    memset(fat32_buf, 0xFF, BLK_BLOCK_SZ);
    put32(fat32_buf + META_ID_OFF + 0u * 4u, 0u);  /* slot 0 → block ID 0 */
    put32(fat32_buf + META_ID_OFF + 1u * 4u, 1u);  /* slot 1 → block ID 1 */
    put32(fat32_buf + META_ID_OFF + 2u * 4u, 5u);  /* slot 2 → block ID 5 */
    put32(fat32_buf + META_ERASE_OFF, 1u);
    flash_range_program(fs_start, fat32_buf, BLK_BLOCK_SZ);

    /* ── Slot 0  (block ID 0): MBR + FAT32 VBR ──────────────────────────── */
    memset(fat32_buf, 0, BLK_BLOCK_SZ);

    /* MBR: single partition entry at offset 0x1BE */
    fat32_buf[0x1BE] = 0x80;                       /* status: bootable       */
    fat32_buf[0x1C2] = 0x0C;                       /* type: FAT32 with LBA   */
    put32(fat32_buf + 0x1C6, 1u);                  /* LBA first sector       */
    put32(fat32_buf + 0x1CA, total_part);          /* partition sector count */
    fat32_buf[0x1FE] = 0x55;
    fat32_buf[0x1FF] = 0xAA;

    /* VBR (FAT32 BPB) at byte offset 512 within this 1-KiB block */
    uint8_t *v = fat32_buf + FAT_SECTOR_SZ;
    put16(v + 11, (uint16_t)FAT_SECTOR_SZ);   /* bytes per sector           */
    v[13] = (uint8_t)SPC;                     /* sectors per cluster        */
    put16(v + 14, (uint16_t)NRESV);           /* reserved sectors           */
    v[16] = (uint8_t)NFATS;                   /* number of FATs             */
    v[21] = 0xF8u;                            /* media: fixed disk          */
    put32(v + 32, total_part);                /* total sectors (32-bit)     */
    put32(v + 36, fat_sz);                    /* sectors per FAT            */
    put32(v + 44, 2u);                        /* root cluster               */
    put16(v + 48, 1u);                        /* FSInfo at partition sec 1  */
    put16(v + 50, 0u);                        /* no backup boot sector      */
    v[510] = 0x55;
    v[511] = 0xAAu;

    write_slot(fs_start, 0u);

    /* ── Slot 1  (block ID 1): FSInfo + FAT1 sector 0 ─────────────────── */
    memset(fat32_buf, 0, BLK_BLOCK_SZ);

    /* FSInfo at partition sector 1 (= first 512 B of this block) */
    put32(fat32_buf + 0,   0x41615252u);   /* lead signature "RRaA"        */
    put32(fat32_buf + 484, 0x61417272u);   /* struct signature "rrAa"      */
    put32(fat32_buf + 488, free_clusters); /* free cluster count           */
    put32(fat32_buf + 492, 3u);            /* next free cluster            */
    put32(fat32_buf + 508, 0xAA550000u);   /* trail signature              */

    /* FAT1 sector 0 at offset 512 — entries 0, 1, 2 are reserved/allocated */
    uint8_t *f = fat32_buf + FAT_SECTOR_SZ;
    put32(f +  0, 0x0FFFFFF8u);  /* entry 0: media marker                  */
    put32(f +  4, 0x0FFFFFFFu);  /* entry 1: EOC                           */
    put32(f +  8, 0x0FFFFFFFu);  /* entry 2: root dir EOC                  */
    /* entries 3..127: 0x00000000 = free (already zeroed by memset)        */

    write_slot(fs_start, 1u);

    /* ── Slot 2  (block ID 5): FAT1[7] zeros + FAT2 sector 0 ──────────── */
    /*
     * FAT1 sectors 1–7 (block IDs 2–4) contain only zero entries (free
     * clusters), so leaving them unwritten is equivalent — the blocks layer
     * returns zeros for unwritten blocks.
     *
     * FAT1 sector 7 (block device sector 10) is all zeros.
     * FAT2 sector 0 (block device sector 11) must mirror FAT1 sector 0.
     */
    memset(fat32_buf, 0, BLK_BLOCK_SZ);

    /* FAT2 sector 0 at offset 512 */
    uint8_t *f2 = fat32_buf + FAT_SECTOR_SZ;
    put32(f2 +  0, 0x0FFFFFF8u);
    put32(f2 +  4, 0x0FFFFFFFu);
    put32(f2 +  8, 0x0FFFFFFFu);

    write_slot(fs_start, 2u);
}
