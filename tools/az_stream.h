/* AZST wire version 1. Native targets are little-endian Windows x64. */
#ifndef AZ_STREAM_H
#define AZ_STREAM_H
#include "../core/az_abi.h"
#include <stddef.h>

enum {
    AZST_MAGIC = 0x54535a41,
    AZST_VERSION = 1,
    AZST_JSON = 1,
    AZST_CREATURES = 2,
    AZST_STATIC = 3,
    AZST_FIELDS = 4,
    AZST_SHAPES = 5,
    AZST_PICTURE = 6,
    AZST_FRAME_BYTES = 16,
    AZST_HEADER_BYTES = 64,
    AZST_RECORD_BYTES = 24,
    AZST_SHAPE_BYTES = 4634,
    AZST_TICK = 0, AZST_HASH_BEFORE = 8, AZST_HASH_AFTER = 16, AZST_COUNT = 24,
    AZST_STRIDE = 28, AZST_GRID = 32, AZST_DELTA = 36, AZST_TICK_US = 40,
    AZST_LIGHT = 44, AZST_CLADES = 48, AZST_FAILURES = 52,
    AZST_MAX_BYTES = 0x7fffffff
};
typedef struct {
    u32 magic;
    u16 type, version;
    u32 length, sequence;
} AzStreamFrame;
typedef struct {
    u64 tick, before, after;
    u32 count, stride;
    u8 grid[3], mode;
    u32 delta, tick_us;
    i8 light[4];
    u32 clades, failures, reserved[2];
} AzStreamHeader;
typedef struct {
    u64 uid;
    u16 x, y, z;
    u8 yaw;
    i8 pitch;
    u8 size, hue, pattern, flags;
    u32 clade;
} AzStreamRecord;
_Static_assert(sizeof(AzStreamFrame) == AZST_FRAME_BYTES, "frame layout");
_Static_assert(sizeof(AzStreamHeader) == AZST_HEADER_BYTES, "header layout");
_Static_assert(offsetof(AzStreamHeader, failures) == AZST_FAILURES, "observer field");
_Static_assert(sizeof(AzStreamRecord) == AZST_RECORD_BYTES, "record layout");
_Static_assert(offsetof(AzStreamRecord, clade) == 20, "record clade");
#endif
