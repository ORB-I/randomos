#pragma once
#include <core/std.h>
#include <drivers/storage/fs.h>

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

typedef struct {
    u32 bg_block_bitmap;
    u32 bg_inode_bitmap;
    u32 bg_inode_table;
    u16 bg_free_blocks_count;
    u16 bg_free_inodes_count;
    u16 bg_used_dirs_count;
    u16 bg_pad;
    char bg_reserved[12];
} __attribute__((packed)) ext2_bg_t;

#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7

#define EXT2_DIR_RECLEN(name_len)  (((name_len) + 8 + 3) & ~3)

typedef struct {
    u32 inode;
    u16 rec_len;
    u16 name_len;
    char name[255];
} __attribute__((packed)) ext2_dir0_t;

typedef struct {
    u32 inode;
    u16 rec_len;
    u8 name_len;
    u8 file_type;
    char name[255];
} __attribute__((packed)) ext2_dir1_t;

#define EXT2_BAD_INO         1
#define EXT2_ROOT_INO        2
#define EXT2_ACL_IDX_INO     3
#define EXT2_ACL_DATA_INO    4
#define EXT2_BOOT_LOADER_INO 5
#define EXT2_UNDEL_DIR_INO   6

#define EXT2_S_IFSOCK 0xC000
#define EXT2_S_IFLNK  0xA000
#define EXT2_S_IFREG  0x8000
#define EXT2_S_IFBLK  0x6000
#define EXT2_S_IFDIR  0x4000
#define EXT2_S_IFCHR  0x2000
#define EXT2_S_IFIFO  0x1000

#define EXT2_S_ISUID 0x0800
#define EXT2_S_ISGID 0x0400
#define EXT2_S_ISVTX 0x0200

#define EXT2_S_IRUSR 0x0100
#define EXT2_S_IWUSR 0x0080
#define EXT2_S_IXUSR 0x0040
#define EXT2_S_IRGRP 0x0020
#define EXT2_S_IWGRP 0x0010
#define EXT2_S_IXGRP 0x0008
#define EXT2_S_IROTH 0x0004
#define EXT2_S_IWOTH 0x0002
#define EXT2_S_IXOTH 0x0001

#define EXT2_SECRM_FL     0x00000001
#define EXT2_UNRM_FL      0x00000002
#define EXT2_COMPR_FL     0x00000004
#define EXT2_SYNC_FL      0x00000008
#define EXT2_IMMUTABLE_FL 0x00000010
#define EXT2_APPEND_FL    0x00000020
#define EXT2_NODUMP_FL    0x00000040
#define EXT2_NOATIME_FL   0x00000080

typedef struct {
    u16 i_mode;
    u16 i_uid;
    u32 i_size;
    u32 i_atime;
    u32 i_ctime;
    u32 i_mtime;
    u32 i_dtime;
    u16 i_gid;
    u16 i_links_count;
    u32 i_blocks;
    u32 i_flags;
    u32 i_osd1;
    u32 i_block[15];
    u32 i_generation;
    u32 i_file_acl;
    u32 i_dir_acl;
    u32 i_faddr;
    char i_osd2[12];
} __attribute__((packed)) ext2_ino_t;

struct ext2_entry {
    ext2_ino_t inod;
    u32 ino;
    usize pos;
    int perms;
};

int _ext2_mount(const char* path);
int _ext2_unmount();
int _ext2_trunc(int fd);
int _ext2_creat(const char* path, u16 mode);
int _ext2_open(const char* path, int flags, u16 mode);
int _ext2_close(int fd);
ssize _ext2_read(int fd, void* buf, usize size);
ssize _ext2_write(int fd, void* buf, usize size);
off_t _ext2_lseek(int fd, off_t off, int whence);
int _ext2_sync(int fd);
int _ext2_opendir(const char* path);
int _ext2_closedir(int dd);
int _ext2_readdir(int dd, struct stat* st);
int _ext2_stat(const char* path, struct stat* st);
int _ext2_unlink(const char* path);
int _ext2_rmdir(const char* path);
int _ext2_rename(const char* oname, const char* nname);
int _ext2_mkdir(const char* path, u16 mode);
int _ext2_chdir(const char* path);
int _ext2_getcwd(char* path, usize len);