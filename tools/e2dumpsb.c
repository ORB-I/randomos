#include "./e2sb.h"
#include <err.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main(int ac, char** av) {
    if (ac < 2) {
        errx(1, "not enough arguments");
    }

    int disk = open(av[1], O_RDONLY);
    if (disk < 0) err(1, "failed to open disk");

    if (lseek(disk, 1024, SEEK_SET) < 0) err(1, "failed to seek");

    u8 rawsb[1024];
    ssize_t ret = read(disk, rawsb, 1024);
    if (ret < 0 || (size_t)ret < 1024) err(1, "failed to read");

    ext2_sb_t* sb = (ext2_sb_t*)rawsb;

    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        errx(1, "bad magic");
    }

    printf("Inode count: %u\n", sb->s_inodes_count);
    printf("Block count: %u\n", sb->s_blocks_count);
    printf("Reserved blocks count: %u\n", sb->s_r_blocks_count);
    printf("Free blocks count: %u\n", sb->s_free_blocks_count);
    printf("Free inodes count: %u\n", sb->s_free_inodes_count);
    printf("First data block: %u\n", sb->s_first_data_block);
    printf("Block size: %llu\n", 1024ULL << (u64)sb->s_log_block_size);
    printf("Fragment size: %llu\n", 1024ULL << (u64)sb->s_log_frag_size);
    printf("Blocks per group: %u\n", sb->s_blocks_per_group);
    printf("Fragments per group: %u\n", sb->s_frags_per_group);
    printf("Inodes per group: %u\n", sb->s_inodes_per_group);
    printf("Mount time: %u\n", sb->s_mtime);
    printf("Write time: %u\n", sb->s_wtime);
    printf("Mount count: %u\n", sb->s_mnt_count);
    printf("Maximum mount count: %u\n", sb->s_max_mnt_count);
    printf("Magic: %04x\n", sb->s_magic);
    printf("State: %u\n", sb->s_state);
    printf("Errors: %u\n", sb->s_errors);
    printf("Revision level: %u.%u\n", sb->s_rev_level, sb->s_minor_rev_level);
    printf("Last check: %u\n", sb->s_lastcheck);
    printf("Check interval: %u\n", sb->s_checkinterval);
    printf("Creator OS: %u\n", sb->s_creator_os);
    printf("Resuid definition: %u\n", sb->s_def_resuid);
    printf("Regid definition: %u\n", sb->s_def_resgid);

    if (sb->s_rev_level == EXT2_DYNAMIC_REV) {
        ext2_dynrev_sb_t* dynsb = (ext2_dynrev_sb_t*)rawsb;
        printf("First inode: %u\n", dynsb->s_first_ino);
        printf("Inode size: %u\n", dynsb->s_inode_size);
        printf("Superblock Block group: %u\n", dynsb->s_block_group_nr);
        printf("Features compat: %x\n", dynsb->s_feature_compat);
        printf("Features incompat: %x\n", dynsb->s_feature_incompat);
        printf("Features rocompat: %x\n", dynsb->s_feature_ro_compat);
    }

    return 0;
}