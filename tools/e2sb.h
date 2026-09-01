#pragma once

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef u64 usize;
typedef s64 ssize;

#define EXT2_SUPER_MAGIC 0xEF53

#define EXT2_VALID_FS 1
#define EXT2_ERROR_FS 2

#define EXT2_ERRORS_CONTINUE 1
#define EXT2_ERRORS_RO       2
#define EXT_ERRORS_PANIC     3

#define EXT2_GOOD_OLD_REV 0
#define EXT2_DYNAMIC_REV  1

#define EXT2_FEATURE_INCOMPAT_COMPRESSION 0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE    0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER     0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV 0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG     0x0010

#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE   0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR    0x0004

typedef struct {
    u32 s_inodes_count;
    u32 s_blocks_count;
    u32 s_r_blocks_count;
    u32 s_free_blocks_count;
    u32 s_free_inodes_count;
    u32 s_first_data_block;
    u32 s_log_block_size;
    u32 s_log_frag_size;
    u32 s_blocks_per_group;
    u32 s_frags_per_group;
    u32 s_inodes_per_group;
    u32 s_mtime;
    u32 s_wtime;
    u16 s_mnt_count;
    u16 s_max_mnt_count;
    u16 s_magic;
    u16 s_state;
    u16 s_errors;
    u16 s_minor_rev_level;
    u32 s_lastcheck;
    u32 s_checkinterval;
    u32 s_creator_os;
    u32 s_rev_level;
    u16 s_def_resuid;
    u16 s_def_resgid;
} __attribute__((packed)) ext2_sb_t;

typedef struct {
    ext2_sb_t sb;
    u32 s_first_ino;
    u16 s_inode_size;
    u16 s_block_group_nr;
    u32 s_feature_compat;
    u32 s_feature_incompat;
    u32 s_feature_ro_compat;
    char s_uuid[16];
    char s_volume_name[16];
    char s_last_mounted[64];
    u32 s_algo_bitmap;
} __attribute__((packed)) ext2_dynrev_sb_t;