#include <core/std.h>
#include <core/lock.h>
#include <core/liballoc.h>
#include <core/printf.h>
#include <core/fd.h>
#include <lib/string.h>
#include <drivers/storage/fs.h>
#include <drivers/storage/fs/ext2.h>
#include <drivers/storage/block/block.h>
#include <drivers/time/gettimeofday.h>
#include <drivers/storage/fs/vfs.h>
#include <scheduler/process.h>
#include <core/errno.h>

// once vfs is implemented, use this as the filesystem private info

static int rdblk(vfs_t* vfs, u32 blk, u8* out) {
    ext2fs_t* fs = EXT2FS(vfs);
    if (blk >= fs->sb.sb.s_blocks_count) return -ERANGE;

    int ret = 0;
    if ((ret = block_read(vfs->blkid, out, blk * fs->spb, fs->spb)) < 0) return ret;
    return 0;
}

static int wrblk(vfs_t* vfs, u32 blk, const u8* in) {
    ext2fs_t* fs = EXT2FS(vfs);
    if (blk >= fs->sb.sb.s_blocks_count) return -ERANGE;

    int ret = 0;
    if ((ret = block_write(vfs->blkid, in, blk * fs->spb, fs->spb)) < 0) return ret;
    return 0;
}

static usize inosz(vfs_t* vfs) {
    ext2fs_t* fs = EXT2FS(vfs);
    if (fs->isdyn) return fs->sb.s_inode_size;
    else return sizeof(ext2_ino_t);
}

static u64 getisize(vfs_t* vfs, ext2_ino_t* inod) {
    ext2fs_t* fs = EXT2FS(vfs);
    if (fs->isdyn && fs->sb.s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_LARGE_FILE) {
        if (inod->i_mode & S_IFDIR) {
            return inod->i_size;
        } else {
            return (u64)inod->i_dir_acl << 32 | (u64)inod->i_size;
        }
    } else {
        return inod->i_size;
    }
}

#define BMP_SET 1
#define BMP_CLR 2
#define BMP_GET 3
static int modbmp(vfs_t* vfs, usize blkid, usize idx, int op) {
    ext2fs_t* fs = EXT2FS(vfs);
    u8 bmp[fs->blocksz];

    int ret = 0;
    if ((ret = rdblk(vfs, blkid, bmp)) < 0) return ret;

    if (op == BMP_SET) {
        bmp[idx / 8] |= (1 << (idx % 8));
    } else if (op == BMP_CLR) {
        bmp[idx / 8] &= ~(1 << (idx % 8));
    } else if (op == BMP_GET) {
        return (bmp[idx / 8] & (1 << (idx % 8)));
    } else {
        return -1;
    }

    if ((ret = wrblk(vfs, blkid, bmp)) < 0) return ret;
    return 0;
}

static int inodeused(vfs_t* vfs, u32 ino) {
    ext2fs_t* fs = EXT2FS(vfs);
    int res = modbmp(vfs, fs->bgs[(ino - 1) / fs->sb.sb.s_inodes_per_group].bg_inode_bitmap, (ino - 1) % fs->sb.sb.s_inodes_per_group, BMP_GET);
    if (res < 0) return res;
    else return res != 0;
}

static int getino(vfs_t* vfs, u32 ino, ext2_ino_t* buf) {
    ext2fs_t* fs = EXT2FS(vfs);
    if (!buf || ino == 0) return -EINVAL;

    int used = inodeused(vfs, ino);
    if (used < 0) return used;
    if (!used) return -ENOENT;

    usize inodsz = inosz(vfs);

    u32 inobg = (ino - 1) / fs->sb.sb.s_inodes_per_group;
    ext2_bg_t* bg = &fs->bgs[inobg];

    usize inoidx = (ino - 1) % fs->sb.sb.s_inodes_per_group;

    usize byteoff = inoidx * inodsz;
    usize inoblk = bg->bg_inode_table + (byteoff / fs->blocksz);

    u8 blk[fs->blocksz];

    int ret = 0;
    if ((ret = rdblk(vfs, inoblk, blk)) < 0) return ret;

    memcpy(buf, blk + (byteoff % fs->blocksz), sizeof(*buf));
    return 0;
}

static int ispow(u32 n, u32 base) {
    if (n == 0) return 0;
    while (n % base == 0) {
        n /= base;
    }
    return n == 1;
}

static int has_sparse(vfs_t* vfs, u32 bg) {
    ext2fs_t* fs = EXT2FS(vfs);
    if (!fs->isdyn || fs->sb.s_feature_ro_compat &~ EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER) {
        return 1;
    } else {
        if (bg == 0 || bg == 1) return 1;
        if (ispow(bg, 3) || ispow(bg, 5) || ispow(bg, 7)) return 1;
    }
    return 0;
}

static int flush_sbs(vfs_t* vfs) {
    ext2fs_t* fs = EXT2FS(vfs);
    int ret = 0;
    for (usize i = 0; i < fs->nbgs; i++) {
        if (has_sparse(vfs, i)) {
            u32 blkid = fs->sb.sb.s_first_data_block + i * fs->sb.sb.s_blocks_per_group;
            u8 buf[fs->blocksz];
            memset(buf, 0, fs->blocksz);

            if (i == 0 && fs->blocksz > 1024) {
                if (rdblk(vfs, 0, buf) < 0) return -1;
                if (!fs->isdyn) {
                    memcpy(buf + 1024, &fs->sb.sb, sizeof(fs->sb.sb));
                } else {
                    memcpy(buf + 1024, &fs->sb, sizeof(fs->sb));
                }
                if (wrblk(vfs, 0, buf) < 0) return -1;
            } else {
                if (!fs->isdyn) {
                    memcpy(buf, &fs->sb.sb, sizeof(fs->sb.sb));
                } else {
                    memcpy(buf, &fs->sb, sizeof(fs->sb));
                }
                if (wrblk(vfs, blkid, buf) < 0) return -1;
            }
            if ((ret = wrblk(vfs, blkid, buf)) < 0) return ret;
        }
    }

    return 0;
}

static int flush_bgs(vfs_t* vfs) {
    ext2fs_t* fs = EXT2FS(vfs);
    int ret = 0;
    for (usize grp = 0; grp < fs->nbgs; grp++) {
        if (has_sparse(vfs, grp)) {
            u32 bgtst = fs->sb.sb.s_first_data_block + grp * fs->sb.sb.s_blocks_per_group + 1;

            usize gpb = fs->gpb;
            for (usize blk = 0; blk < fs->bgtbln; blk++) {
                usize fst = blk * gpb;
                if (fst >= fs->nbgs) break;

                usize cnt = fs->nbgs - fst;
                if (cnt > gpb) cnt = gpb;
                u8 buf[fs->blocksz];
                memset(buf, 0, fs->blocksz);
                memcpy(buf, &fs->bgs[fst], cnt * sizeof(ext2_bg_t));
                if ((ret = wrblk(vfs, bgtst + blk, buf)) < 0) return ret;
            }
        }
    }

    return 0;
}

static int flush_inode(vfs_t* vfs, u64 ino, ext2_ino_t* inod) {
    if (!inod || ino == 0) return -EINVAL;
    ext2fs_t* fs = EXT2FS(vfs);

    int used = inodeused(vfs, ino);
    if (used < 0) return used;
    if (!used) return -ENOENT;

    usize inodsz = inosz(vfs);

    u32 inobg = (ino - 1) / fs->sb.sb.s_inodes_per_group;
    ext2_bg_t* bg = &fs->bgs[inobg];

    usize inoidx = (ino - 1) % fs->sb.sb.s_inodes_per_group;

    usize byteoff = inoidx * inodsz;
    usize inoblk = bg->bg_inode_table + (byteoff / fs->blocksz);

    u8 blk[fs->blocksz];

    int ret = 0;
    if ((ret = rdblk(vfs, inoblk, blk)) != 0) return ret;

    memcpy(blk + (byteoff % fs->blocksz), inod, sizeof(*inod));
    if ((ret = wrblk(vfs, inoblk, blk)) < 0) return ret;

    return 0;
}

static int read_indr(vfs_t* vfs, u32 blkn, u32* obuf) {
    ext2fs_t* fs = EXT2FS(vfs);
    if (blkn == 0) {
        memset(obuf, 0, fs->blocksz);
        return 0;
    }
    return rdblk(vfs, blkn, (u8*)obuf);
}

static ssize ino_getblkid(vfs_t* vfs, ext2_ino_t* inod, usize idx) {
    ext2fs_t* fs = EXT2FS(vfs);
    if (!inod) return -EINVAL;

    const u32 ppb = fs->ppb;
    u32 indrs[3][fs->ppb];

    int ret = 0;
    if (idx < 12) {
        return inod->i_block[idx];
    } else if (idx < 12 + ppb) {
        if ((ret = read_indr(vfs, inod->i_block[12], indrs[0])) < 0) return ret;
        return indrs[0][idx-12];
    } else if (idx < 12 + ppb + (ppb * ppb)) {
        u32 ridx = idx - 12 - ppb;
        if ((ret = read_indr(vfs, inod->i_block[13], indrs[0])) < 0) return ret;
        if ((ret = read_indr(vfs, indrs[0][ridx / ppb], indrs[1])) < 0) return ret;
        return indrs[1][ridx % ppb];
    } else {
        u32 ridx = idx - 12 - ppb - (ppb * ppb);

        if ((ret = read_indr(vfs, inod->i_block[14], indrs[0])) < 0) return ret;
        if ((ret = read_indr(vfs, indrs[0][ridx / (ppb * ppb)], indrs[1])) < 0) return ret;
        if ((ret = read_indr(vfs, indrs[1][(ridx / ppb) % ppb], indrs[2])) < 0) return ret;

        return indrs[2][ridx % ppb];
    }
}

static int membeq(void* ptr, u8 b, usize sz) {
    for (usize i = 0; i < sz; i++) {
        if (((char*)ptr)[i] != b) return 0;
    }
    return 1;
}

static ssize allocblk(vfs_t* vfs, u32 bg) {
    ext2fs_t* fs = EXT2FS(vfs);
    if (bg >= fs->nbgs) return -EINVAL;
    int ret = 0;
    for (usize n = 0; n < fs->nbgs; n++) {
        u32 grp = (bg + n) % fs->nbgs;
        u8 bmp[fs->blocksz];
        if (fs->bgs[grp].bg_free_blocks_count == 0) continue;
        if ((ret = rdblk(vfs, fs->bgs[grp].bg_block_bitmap, bmp)) < 0) return ret;
    
        for (usize i = 0; i < (fs->sb.sb.s_blocks_per_group + 7) / 8 && i < fs->blocksz; i++) {
            for (usize b = 0; b < 8; b++) {
                usize idx = i * 8 + b;
                if (idx >= fs->sb.sb.s_blocks_per_group) break;
                if (!(bmp[i] & (1 << b))) {
                    bmp[i] |= (1 << b);
                    if ((ret = wrblk(vfs, fs->bgs[grp].bg_block_bitmap, bmp)) < 0) return ret;

                    fs->bgs[grp].bg_free_blocks_count--;
                    fs->sb.sb.s_free_blocks_count--;
                    
                    if ((ret = flush_bgs(vfs)) < 0) return ret;
                    if ((ret = flush_sbs(vfs)) < 0) return ret;
                    return fs->sb.sb.s_first_data_block + grp * fs->sb.sb.s_blocks_per_group + idx;
                }
            }
        }
    }
    return -ENOSPC;
}

static ssize ino_allocblk(vfs_t* vfs, u32 ino, ext2_ino_t* inod) {
    ext2fs_t* fs = EXT2FS(vfs);
    u32 inobg = (ino - 1) / fs->sb.sb.s_inodes_per_group;
    int ret = 0;
    u32 spb = fs->spb;

    for (usize i = 0; i < 12; i++) {
        if (inod->i_block[i] == 0) {
            ssize blk = allocblk(vfs, inobg);
            if (blk < 0) return blk;
            inod->i_block[i] = blk;
            inod->i_blocks += spb;
            if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
            return blk;
        }
    }

    const u32 ppb = fs->ppb;
    u32 indrs[3][fs->ppb];

    if (inod->i_block[12] == 0) {
        ssize blk = allocblk(vfs, inobg);
        if (blk < 0) return blk;
        inod->i_block[12] = blk;
        inod->i_blocks += spb;
        if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
    }

    if ((ret = read_indr(vfs, inod->i_block[12], indrs[0])) < 0) return ret;
    for (usize i = 0; i < ppb; i++) {
        if (indrs[0][i] == 0) {
            ssize blk = allocblk(vfs, inobg);
            if (blk < 0) return blk;
            indrs[0][i] = ret;
            inod->i_blocks += spb;
            if ((ret = wrblk(vfs, inod->i_block[12], (u8*)indrs[0])) < 0) return ret;
            if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
            return blk;
        }
    }

    if (inod->i_block[13] == 0) {
        ssize blk = allocblk(vfs, inobg);
        if (blk < 0) return blk;
        inod->i_block[13] = blk;
        inod->i_blocks += spb;
        if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
    }

    if ((ret = read_indr(vfs, inod->i_block[13], indrs[0])) < 0) return ret;
    for (usize i = 0; i < ppb; i++) {
        if (indrs[0][i] == 0) {
            ssize blk = allocblk(vfs, inobg);
            if (blk < 0) return blk;
            indrs[0][i] = blk;
            inod->i_blocks += spb;
            if ((ret = wrblk(vfs, inod->i_block[13], (u8*)indrs[0])) < 0) return ret;
            if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
        }

        if ((ret = read_indr(vfs, indrs[0][i], indrs[1])) < 0) return ret;
        for (usize j = 0; j < ppb; j++) {
            if (indrs[1][j] == 0) {
                ssize blk = allocblk(vfs, inobg);
                if (blk < 0) return blk;
                indrs[1][j] = blk;
                inod->i_blocks += spb;
                if ((ret = wrblk(vfs, indrs[0][i], (u8*)indrs[1])) < 0) return ret;
                if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
                return blk;
            }
        }
    }

    if (inod->i_block[14] == 0) {
        ssize blk = allocblk(vfs, inobg);
        if (blk < 0) return blk;
        inod->i_block[14] = blk;
        inod->i_blocks += spb;
        if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
    }

    if ((ret = read_indr(vfs, inod->i_block[14], indrs[0])) < 0) return ret;
    for (usize i = 0; i < ppb; i++) {
        if (indrs[0][i] == 0) {
            ssize blk = allocblk(vfs, inobg);
            if (blk < 0) return blk;
            indrs[0][i] = blk;
            inod->i_blocks += spb;
            if ((ret = wrblk(vfs, inod->i_block[14], (u8*)indrs[0])) < 0) return ret;
            if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
        }

        if ((ret = read_indr(vfs, indrs[0][i], indrs[1])) < 0) return ret;
        for (usize j = 0; j < ppb; j++) {
            if (indrs[1][j] == 0) {
                ssize blk = allocblk(vfs, inobg);
                if (blk < 0) return blk;
                indrs[1][j] = blk;
                inod->i_blocks += spb;
                if ((ret = wrblk(vfs, indrs[0][j], (u8*)indrs[1])) < 0) return ret;
                if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
            }

            if ((ret = read_indr(vfs, indrs[1][j], indrs[2])) < 0) return ret;
            for (usize k = 0; k < ppb; k++) {
                if (indrs[2][k] == 0) {
                    ssize blk = allocblk(vfs, inobg);
                    if (blk < 0) return blk;
                    indrs[2][k] = blk;
                    inod->i_blocks += spb;
                    if ((ret = wrblk(vfs, indrs[1][j], (u8*)indrs[2])) < 0) return ret;
                    if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
                    return blk;
                }
            }
        }
    }

    return -ENOSPC;
}

static int freeblk(vfs_t* vfs, u32 blkid) {
    ext2fs_t* fs = EXT2FS(vfs);
    if (blkid == 0) return 0;
    u32 bg = (blkid - fs->sb.sb.s_first_data_block) / fs->sb.sb.s_blocks_per_group;
    u32 bmpidx = (blkid - fs->sb.sb.s_first_data_block) % fs->sb.sb.s_blocks_per_group;

    u8 zero[fs->blocksz];
    memset(zero, 0, fs->blocksz);
    wrblk(vfs, blkid, zero);

    int ret = 0;
    if ((ret = modbmp(vfs, fs->bgs[bg].bg_block_bitmap, bmpidx, BMP_CLR)) < 0) return ret;
    fs->bgs[bg].bg_free_blocks_count++;
    fs->sb.sb.s_free_blocks_count++;

    if ((ret = flush_bgs(vfs)) < 0) return ret;
    if ((ret = flush_sbs(vfs)) < 0) return ret;

    return 0;
}

static int ino_freeblk(vfs_t* vfs, u32 ino, ext2_ino_t* inod, usize idx) {
    ext2fs_t* fs = EXT2FS(vfs);
    const u32 ppb = fs->ppb;
    u32 spb = fs->spb;
    u32 indrs[3][fs->ppb];

    int ret = 0;
    if (idx < 12) {
        if (inod->i_block[idx] != 0) {
            if ((ret = freeblk(vfs, inod->i_block[idx])) < 0) return ret;
            inod->i_block[idx] = 0;
            if (inod->i_blocks >= spb) inod->i_blocks -= spb;
            else inod->i_blocks = 0;
            if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
        }
    } else if (idx < 12 + ppb) {
        if ((ret = read_indr(vfs, inod->i_block[12], indrs[0])) < 0) return ret;
        usize pblk = indrs[0][idx-12];

        if (pblk != 0) {
            if ((ret = freeblk(vfs, pblk)) < 0) return ret;
            indrs[0][idx-12] = 0;
            if (inod->i_blocks >= spb) inod->i_blocks -= spb;
            else inod->i_blocks = 0;

            if (membeq(indrs[0], 0, fs->blocksz)) {
                if ((ret = freeblk(vfs, inod->i_block[12])) < 0) return ret;
                inod->i_block[12] = 0;
                if (inod->i_blocks >= spb) inod->i_blocks -= spb;
                else inod->i_blocks = 0;
            } else {
                if ((ret = wrblk(vfs, inod->i_block[12], (u8*)indrs[0])) < 0) return ret;
            }

            if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
        }
    } else if (idx < 12 + ppb + (ppb * ppb)) {
        u32 ridx = idx - 12 - ppb;
        if ((ret = read_indr(vfs, inod->i_block[13], indrs[0])) < 0) return ret;
        if ((ret = read_indr(vfs, indrs[0][ridx / ppb], indrs[1])) < 0) return ret;

        usize pblk = indrs[1][ridx % ppb];
        if (pblk != 0) {
            if ((ret = freeblk(vfs, pblk)) < 0) return ret;
            indrs[1][ridx % ppb] = 0;
            if (inod->i_blocks >= spb) inod->i_blocks -= spb;
            else inod->i_blocks = 0;

            if (membeq(indrs[1], 0, fs->blocksz)) {
                if ((ret = freeblk(vfs, indrs[0][ridx / ppb])) < 0) return ret;
                indrs[0][ridx / ppb] = 0;
                if (inod->i_blocks >= spb) inod->i_blocks -= spb;
                else inod->i_blocks = 0;

                if (membeq(indrs[0], 0, fs->blocksz)) {
                    if ((ret = freeblk(vfs, inod->i_block[13])) < 0) return ret;
                    inod->i_block[13] = 0;
                    if (inod->i_blocks >= spb) inod->i_blocks -= spb;
                    else inod->i_blocks = 0;
                } else {
                    if ((ret = wrblk(vfs, inod->i_block[13], (u8*)indrs[0])) < 0) return ret;
                }
            } else {
                if ((ret = wrblk(vfs, indrs[0][ridx / ppb], (u8*)indrs[1])) < 0) return ret;
            }

            if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
        }
    } else {
        u32 ridx = idx - 12 - ppb - (ppb * ppb);

        if ((ret = read_indr(vfs, inod->i_block[14], indrs[0])) < 0) return ret;
        if ((ret = read_indr(vfs, indrs[0][ridx / (ppb * ppb)], indrs[1])) < 0) return ret;
        if ((ret = read_indr(vfs, indrs[1][(ridx / ppb) % ppb], indrs[2])) < 0) return ret;

        usize pblk = indrs[2][ridx % ppb];
        if (pblk != 0) {
            if ((ret = freeblk(vfs, pblk)) < 0) return ret;
            indrs[2][ridx % ppb] = 0;
            if (inod->i_blocks >= spb) inod->i_blocks -= spb;
            else inod->i_blocks = 0;

            if (membeq(indrs[2], 0, fs->blocksz)) {
                if ((ret = freeblk(vfs, indrs[1][(ridx / ppb) % ppb])) < 0) return ret;
                indrs[1][(ridx / ppb) % ppb] = 0;
                if (inod->i_blocks >= spb) inod->i_blocks -= spb;
                else inod->i_blocks = 0;

                if (membeq(indrs[1], 0, fs->blocksz)) {
                    if ((ret = freeblk(vfs, indrs[0][ridx / (ppb * ppb)])) < 0) return ret;
                    indrs[0][ridx / (ppb * ppb)] = 0;
                    if (inod->i_blocks >= spb) inod->i_blocks -= spb;
                    else inod->i_blocks = 0;

                    if (membeq(indrs[0], 0, fs->blocksz)) {
                        if ((ret = freeblk(vfs, inod->i_block[14])) < 0) return ret;
                        inod->i_block[14] = 0;
                        if (inod->i_blocks >= spb) inod->i_blocks -= spb;
                        else inod->i_blocks = 0;
                    } else {
                        if ((ret = wrblk(vfs, inod->i_block[14], (u8*)indrs[0])) < 0) return ret;
                    }
                } else {
                    if ((ret = wrblk(vfs, indrs[0][ridx / (ppb * ppb)], (u8*)indrs[1])) < 0) return ret;
                }
            } else {
                if ((ret = wrblk(vfs, indrs[1][(ridx / ppb) % ppb], (u8*)indrs[2])) < 0) return ret;
            }

            if ((ret = flush_inode(vfs, ino, inod)) < 0) return ret;
        }
    }

    return 0;
}

static u32 decode_dev(ext2_ino_t* inod) {
    if (inod->i_block[0]) return EXT2_OLD_DEV_DECODE(inod->i_block[0]);
    else return EXT2_NEW_DEV_DECODE(inod->i_block[1]);
}

static void encode_dev(ext2_ino_t* inod, u32 dev) {
    if (EXT2_OLD_DEV_VALID(dev)) inod->i_block[0] = EXT2_OLD_DEV_ENCODE(dev);
    else inod->i_block[1] = EXT2_NEW_DEV_ENCODE(dev);
}

static int mkino_base(vfs_t* vfs, ext2_ino_t* inode) {
        ext2fs_t* fs = EXT2FS(vfs);
    int ret = 0;

    for (usize grp = 0; grp < fs->nbgs; grp++) {
        u8 bmp[fs->blocksz];
        if (fs->bgs[grp].bg_free_inodes_count == 0) continue;
        if ((ret = rdblk(vfs, fs->bgs[grp].bg_inode_bitmap, bmp)) < 0) return ret;

        for (usize i = 0; i < (fs->sb.sb.s_inodes_per_group + 7) / 8 && i < fs->blocksz; i++) {
            for (usize b = 0; b < 8; b++) {
                if (!(bmp[i] & (1 << b))) {
                    usize idx = i * 8 + b;
                    if (idx >= fs->sb.sb.s_inodes_per_group) break;
                    bmp[i] |= (1 << b);
                    if ((ret = wrblk(vfs, fs->bgs[grp].bg_inode_bitmap, bmp)) < 0) return ret;

                    fs->bgs[grp].bg_free_inodes_count--;
                    fs->sb.sb.s_free_inodes_count--;

                    if ((ret = flush_bgs(vfs)) < 0) return ret;
                    if ((ret = flush_sbs(vfs)) < 0) return ret;

                    u32 ino = grp * fs->sb.sb.s_inodes_per_group + idx + 1;
                    if ((ret = flush_inode(vfs, ino, inode)) < 0) return ret;

                    return ino;
                }
            }
        }
    }

    return -ENOSPC;
}

static u8* getinodata(vfs_t* vfs, ext2_ino_t* inode, usize* outsz, int* status) {
    ext2fs_t* fs = EXT2FS(vfs);
    if (!inode || !outsz) {
        *status = -EINVAL;
        return NULL;
    }

    u32 tsz = getisize(vfs, inode);
    *outsz = tsz;
    if (tsz == 0) {
        *status = 0;
        return NULL;
    }

    u8* obuf = malloc(tsz);
    if (!obuf) {
        *status = -ENOMEM;
        return NULL;
    }

    u32 tblks = (tsz + fs->blocksz - 1) / fs->blocksz;

    u32 bidx = 0;
    int ret = 0;
    while (bidx < tblks) {
        ssize pblk = ino_getblkid(vfs, inode, bidx++);
        if (pblk < 0) {
            *status = pblk;
            goto err;
        }

        usize blft = tsz - ((bidx - 1) * fs->blocksz);
        usize b2cp = (blft > fs->blocksz) ? fs->blocksz : blft;
        u8* dptr = obuf + ((bidx - 1) * fs->blocksz);

        if (pblk == 0) {
            memset(dptr, 0, b2cp);
        } else if (b2cp == fs->blocksz) {
            if ((ret = rdblk(vfs, pblk, dptr)) < 0) {
                *status = ret;
                goto err;
            }
        } else {
            u8 tmp[fs->blocksz];
            if ((ret = rdblk(vfs, pblk, tmp)) < 0) {
                *status = ret;
                goto err;
            }
            memcpy(dptr, tmp, b2cp);
        }
    }

    return obuf;
err:
    *outsz = 0;
    free(obuf);
    return NULL;
}

ssize ext2fs_lookup(vfs_t* vfs, u32 dino, const char* name) {
    ext2_ino_t dinod;
    int ret = 0;
    if ((ret = getino(vfs, dino, &dinod)) < 0) return ret;

    usize dirsz = 0;
    u8* ddata = getinodata(vfs, &dinod, &dirsz, &ret);
    if (!ddata) return ret;

    for (usize i = 0; i < dirsz;) {
        // we can use this even though in dir0_t namelen is u16
        // because you can only have up to 255 chars for name or UINT8_MAX
        ext2_dir1_t* dir = (ext2_dir1_t*)&ddata[i];
        if (dir->name_len == strlen(name) && memcmp(dir->name, name, dir->name_len) == 0) {
            u32 inode = dir->inode;
            free(ddata);
            return inode;
        }

        i += dir->rec_len;
    }

    free(ddata);
    return -ENOENT;
}

ssize ext2fs_mkino(vfs_t* vfs, u16 mode, u16 uid, u16 gid) {
    u64 time = gettimeofday();

    ext2_ino_t inode = {
        mode, uid, 0,
        time, time, time, 0,
        gid, 0, 0, 0,
        0, {0}, 0, 0,
        0, 0, {0}
    };

    return mkino_base(vfs, &inode);
}

ssize ext2fs_mknod(vfs_t* vfs, u16 mode, u16 uid, u16 gid, u32 rdev) {
    u64 time = gettimeofday();

    ext2_ino_t inode = {
        mode, uid, 0,
        time, time, time, 0,
        gid, 0, 0, 0,
        0, {0}, 0, 0,
        0, 0, {0}
    };

    encode_dev(&inode, rdev);
    return mkino_base(vfs, &inode);
}

ssize ext2fs_rmlink(vfs_t* vfs, u32 dino, const char* name) {
    ext2fs_t* fs = EXT2FS(vfs);
    int ret = 0;

    ext2_ino_t dinod;
    if ((ret = getino(vfs, dino, &dinod)) < 0) return ret;

    usize nblks = (getisize(vfs, &dinod) + fs->blocksz - 1) / fs->blocksz;
    for (usize b = 0; b < nblks; b++) {
        ssize blk = ino_getblkid(vfs, &dinod, b);
        if (blk < 0) return blk;

        u8 blkd[fs->blocksz];
        if (rdblk(vfs, blk, blkd) < 0) return -1;

        ext2_dir1_t* pdir = NULL;
        for (usize i = 0; i < fs->blocksz;) {
            ext2_dir1_t* dir_entry = (ext2_dir1_t*)&blkd[i];
            if (dir_entry->rec_len < 8 || i + dir_entry->rec_len > fs->blocksz) return -EINVAL;
            if (dir_entry->name_len == strlen(name) && memcmp(dir_entry->name, name, dir_entry->name_len) == 0) {
                u32 ino = dir_entry->inode;
                ext2_ino_t inod;
                if ((ret = getino(vfs, ino, &inod)) < 0) return ret;
                if (pdir) pdir->rec_len += dir_entry->rec_len;
                memset(dir_entry, 0, dir_entry->rec_len);
                if ((ret = wrblk(vfs, blk, blkd)) < 0) return ret;
                inod.i_links_count--;
                if ((ret = flush_inode(vfs, ino, &inod)) < 0) return ret;
                return 0;
            }
            pdir = dir_entry;
            i += dir_entry->rec_len;
        }
    }

    return -ENOENT;
}

ssize ext2fs_mklink(vfs_t* vfs, u32 ino, u16 mode, u32 dino, const char* name) {
    ext2fs_t* fs = EXT2FS(vfs);
    int ret = 0;

    ext2_ino_t inod;
    if ((ret = getino(vfs, ino, &inod)) < 0) return ret;

    ext2_ino_t parinod;
    if ((ret = getino(vfs, dino, &parinod)) < 0) return ret;

    ext2_dir1_t newent = {
        ino, 0, (u8)strlen(name), 0, {0}
    };
    memcpy(newent.name, name, strlen(name));
    
    if (fs->isdyn) {
        u16 type = EXT2_S_TYPE(mode);
        if (type == EXT2_S_IFSOCK) {
            newent.file_type = EXT2_FT_SOCK;
        } else if (type == EXT2_S_IFLNK) {
            newent.file_type = EXT2_FT_SYMLINK;
        } else if (type == EXT2_S_IFREG) {
            newent.file_type = EXT2_FT_REG_FILE;
        } else if (type == EXT2_S_IFBLK) {
            newent.file_type = EXT2_FT_BLKDEV;
        } else if (type == EXT2_S_IFDIR) {
            newent.file_type = EXT2_FT_DIR;
        } else if (type == EXT2_S_IFCHR) {
            newent.file_type = EXT2_FT_CHRDEV;
        } else if (type == EXT2_S_IFIFO) {
            newent.file_type = EXT2_FT_FIFO;
        } else {
            newent.file_type = EXT2_FT_UNKNOWN;
        }
    }

    usize nsz = EXT2_DIR_RECLEN(strlen(name));
    usize nblks = (getisize(vfs, &parinod) + fs->blocksz - 1) / fs->blocksz;
    for (usize b = 0; b < nblks; b++) {
        ssize blk = ino_getblkid(vfs, &parinod, b);
        if (blk < 0) return blk;

        u8 blkd[fs->blocksz];
        if ((ret = rdblk(vfs, blk, blkd)) < 0) return ret;
        for (usize i = 0; i < fs->blocksz;) {
            ext2_dir1_t* ent = (ext2_dir1_t*)&blkd[i];
            if (ent->rec_len < 8 || i + ent->rec_len > 1024) return -EINVAL;
            usize oldsz = EXT2_DIR_RECLEN(ent->name_len);

            if (ent->inode == 0) {
                if (ent->rec_len >= nsz) {
                    newent.rec_len = ent->rec_len;
                    memcpy(&blkd[i], &newent, nsz);
                    if ((ret = wrblk(vfs, blk, blkd)) < 0) return ret;
                    inod.i_links_count++;
                    if ((ret = flush_inode(vfs, ino, &inod)) < 0) return ret;
                    return 0;
                }
            } else if (ent->rec_len >= oldsz && ent->rec_len - oldsz >= nsz) {
                newent.rec_len = ent->rec_len - oldsz;
                ent->rec_len = oldsz;
                memcpy(&blkd[i + oldsz], &newent, nsz);
                if ((ret = wrblk(vfs, blk, blkd)) < 0) return ret;
                inod.i_links_count++;
                if ((ret = flush_inode(vfs, ino, &inod)) < 0) return ret;
                return 0;
            }

            i += ent->rec_len;
        }
    }

    ssize nblk = ino_allocblk(vfs, dino, &parinod);
    if (nblk < 0) return nblk;

    u8 blkd[fs->blocksz];
    memset(blkd, 0, fs->blocksz);
    newent.rec_len = (u16)fs->blocksz;
    memcpy(blkd, &newent, EXT2_DIR_RECLEN(strlen(name)));
    parinod.i_size += fs->blocksz;
    if ((ret = wrblk(vfs, nblk, blkd)) < 0) return ret;
    if ((ret = flush_inode(vfs, dino, &parinod)) < 0) return ret;
    inod.i_links_count++;
    if ((ret = flush_inode(vfs, ino, &inod)) < 0) return ret;
    return 0;
}

ssize ext2fs_umount(vfs_t* vfs) {
    ext2fs_t* fs = EXT2FS(vfs);
    if (!fs) return 0;
    if (fs->bgs) free(fs->bgs);
    if (fs) free(fs);
    if (vfs->ops) free(vfs->ops);
    return 0;
}

ssize ext2fs_read(vfs_t* vfs, u32 ino, usize off, usize nb, void* buf) {
    ext2fs_t* fs = EXT2FS(vfs);
    int ret = 0;

    ext2_ino_t inod;
    if ((ret = getino(vfs, ino, &inod)) < 0) return ret;

    usize nread = 0;
    while (nread < nb) {
        usize pos = off + nread;
        usize blk = pos / fs->blocksz;
        usize off = pos % fs->blocksz;

        ssize blkid = ino_getblkid(vfs, &inod, blk);
        if (blkid < 0) return nread;

        u8 blkd[fs->blocksz];
        if ((ret = rdblk(vfs, blkid, blkd)) < 0) return ret;

        usize n = fs->blocksz - off;
        if (n > nb - nread) n = nb - nread;
        memcpy((u8*)buf + nread, blkd + off, n);
        nread += n;
    }
    return nread;
}

ssize ext2fs_write(vfs_t* vfs, u32 ino, usize off, usize nb, void* buf) {
    ext2fs_t* fs = EXT2FS(vfs);
    int ret = 0;

    ext2_ino_t inod;
    if ((ret = getino(vfs, ino, &inod)) < 0) return ret;

    usize nwritten = 0;
    while (nwritten < nb) {
        usize pos = off + nwritten;
        usize blk = pos / fs->blocksz;
        usize off = pos % fs->blocksz;

        ssize blkid = ino_getblkid(vfs, &inod, blk);
        if (blkid < 0) {
            if ((blkid = ino_allocblk(vfs, ino, &inod)) < 0) {
                inod.i_size += nwritten;
                if ((ret = flush_inode(vfs, ino, &inod)) < 0) return ret;
                return nwritten;
            }
        }

        u8 blkd[fs->blocksz];
        if ((ret = rdblk(vfs, blkid, blkd)) < 0) {
            inod.i_size += nwritten;
            if ((ret = flush_inode(vfs, ino, &inod)) < 0) return ret;
            return ret;
        }

        usize n = fs->blocksz - off;
        if (n > nb - nwritten) n = nb - nwritten;
        memcpy(blkd + off, (u8*)buf + nwritten, n);
        if ((ret = wrblk(vfs, blkid, blkd)) < 0) {
            inod.i_size += nwritten;
                if ((ret = flush_inode(vfs, ino, &inod)) < 0) return ret;
            return ret;
        }

        nwritten += n;
    }

    inod.i_size += nwritten;
    if ((ret = flush_inode(vfs, ino, &inod)) < 0) return ret;
    return nwritten;
}

ssize ext2fs_readdir(vfs_t* vfs, u32 dino, u64* prv, char* name, usize namlen, vinode_t* buf) {
    ext2fs_t* fs = EXT2FS(vfs);
    int ret = 0;

    ext2_ino_t inod;
    if ((ret = getino(vfs, dino, &inod)) < 0) return ret;

    usize nblks = (getisize(vfs, &inod) + fs->blocksz - 1) / fs->blocksz;
    usize pos = 0;

    for (usize b = 0; b < nblks; b++) {
        ssize blk = ino_getblkid(vfs, &inod, b);
        if (blk < 0) return blk;

        u8 blkd[fs->blocksz];
        if ((ret = rdblk(vfs, blk, blkd)) < 0) return ret;
        for (usize i = 0; i < 1024;) {
            ext2_dir1_t* dir = (ext2_dir1_t*)&blkd[i];
            if (dir->rec_len < 8 || i + dir->rec_len > fs->blocksz) return -EINVAL;

            if (pos == *prv) {
                if (dir->inode == 0) {
                    return -ENOENT;
                } else {
                    ext2_ino_t inod;
                    if ((ret = getino(vfs, dir->inode, &inod)) < 0) return ret;
                    
                    buf->mode = inod.i_mode;
                    buf->uid = inod.i_uid;
                    buf->atime = inod.i_atime;
                    buf->ctime = inod.i_ctime;
                    buf->mtime = inod.i_mtime;
                    buf->gid = inod.i_gid;
                    buf->lnkcnt = inod.i_links_count;
                    buf->size = getisize(vfs, &inod);
                    buf->priv = dir->inode;

                    if (S_TYPE(inod.i_mode) == S_IFCHR || S_TYPE(inod.i_mode) == S_IFBLK) {
                        buf->rdev = decode_dev(&inod);
                    }

                    if (namlen >= dir->name_len + 1) {
                        memcpy(name, dir->name, dir->name_len);
                        name[dir->name_len] = '\0';
                    }

                    (*prv)++;
                    return dir->inode;
                }
            }

            pos++;
            i += dir->rec_len;
        }
    }

    return -1;
}

ssize ext2fs_getino(vfs_t* vfs, u32 ino, vinode_t* buf) {
    int ret = 0;

    ext2_ino_t inod;
    if ((ret = getino(vfs, ino, &inod)) < 0) return ret;

    buf->mode = inod.i_mode;
    buf->uid = inod.i_uid;
    buf->atime = inod.i_atime;
    buf->ctime = inod.i_ctime;
    buf->mtime = inod.i_mtime;
    buf->gid = inod.i_gid;
    buf->lnkcnt = inod.i_links_count;
    buf->size = getisize(vfs, &inod);
    buf->priv = ino;

    if (S_TYPE(inod.i_mode) == S_IFCHR || S_TYPE(inod.i_mode) == S_IFBLK) {
        buf->rdev = decode_dev(&inod);
    }

    return 0;
}

ssize ext2fs_setino(vfs_t* vfs, u32 ino, vinode_t* buf) {
    int ret = 0;
    
    ext2_ino_t inod;
    if ((ret = getino(vfs, ino, &inod)) < 0) return ret;

    inod.i_mode = buf->mode;
    inod.i_uid = buf->uid;
    inod.i_atime = buf->atime;
    inod.i_ctime = buf->ctime;
    inod.i_mtime = buf->mtime;
    inod.i_gid = buf->gid;
    inod.i_links_count = buf->lnkcnt;
    inod.i_size = buf->size;

    if (S_TYPE(buf->mode) == S_IFCHR || S_TYPE(buf->mode) == S_IFBLK) {
        encode_dev(&inod, buf->rdev);
    }

    return flush_inode(vfs, ino, &inod);
}

ssize ext2fs_trunc(vfs_t* vfs, u32 ino) {
    ext2fs_t* fs = EXT2FS(vfs);
    int ret = 0;

    ext2_ino_t inod;
    if ((ret = getino(vfs, ino, &inod)) < 0) return ret;

    usize nblks = (getisize(vfs, &inod) + fs->blocksz - 1) / fs->blocksz;
    for (usize i = 0; i < nblks; i++) {
        if ((ret = ino_freeblk(vfs, ino, &inod, i)) < 0) return ret;
    }

    return 0;
}

ssize ext2fs_rmino(vfs_t* vfs, u32 ino) {
    ext2fs_t* fs = EXT2FS(vfs);
    int ret = 0;

    ext2_ino_t inod;
    if ((ret = getino(vfs, ino, &inod)) < 0) return ret;

    usize nblks = (getisize(vfs, &inod) + fs->blocksz - 1) / fs->blocksz;
    for (usize i = 0; i < nblks; i++) {
        if ((ret = ino_freeblk(vfs, ino, &inod, i)) < 0) return ret;
    }

    memset(&inod, 0, sizeof(inod));
        
    u32 inobg = (ino - 1) / fs->sb.sb.s_inodes_per_group;
    ext2_bg_t* bg = &fs->bgs[inobg];

    bg->bg_free_inodes_count++;
    fs->sb.sb.s_free_inodes_count++;
    if ((ret = modbmp(vfs, bg->bg_inode_bitmap, (ino - 1) % fs->sb.sb.s_inodes_per_group, BMP_CLR)) < 0) return ret;

    if ((ret = flush_inode(vfs, ino, &inod)) < 0) return ret;
    if ((ret = flush_bgs(vfs)) < 0) return ret;
    if ((ret = flush_sbs(vfs)) < 0) return ret;

    return 0;
}

int ext2fs_mount_setops(vfs_t* vfs) {
    vfs->ops = malloc(sizeof(vfsops_t));
    if (!vfs->ops) {
        return -ENOMEM;
    }

    vfs->ops->umount = ext2fs_umount;
    vfs->ops->lookup = ext2fs_lookup;
    vfs->ops->readdir = ext2fs_readdir;
    vfs->ops->mkino = ext2fs_mkino;
    vfs->ops->rmino = ext2fs_rmino;
    vfs->ops->getino = ext2fs_getino;
    vfs->ops->setino = ext2fs_setino;
    vfs->ops->mklink = ext2fs_mklink;
    vfs->ops->rmlink = ext2fs_rmlink;
    vfs->ops->trunc = ext2fs_trunc;
    vfs->ops->read = ext2fs_read;
    vfs->ops->write = ext2fs_write;
    vfs->ops->mknod = ext2fs_mknod;

    return 0;
}

int ext2fs_mount(vfs_t* vfs) {
    int ret = 0;

    u8 rawsb[1024];
    if ((ret = block_read(vfs->blkid, rawsb, 2, 2)) < 0) return ret;
    ext2_sb_t* sb = (ext2_sb_t*)rawsb;

    if (sb->s_magic != EXT2_SUPER_MAGIC) return -EINVAL;
    if (sb->s_errors != EXT2_ERRORS_CONTINUE) return -EINVAL;

    ext2fs_t* fs = malloc(sizeof(ext2fs_t));
    if (!fs) return -ENOMEM;
    vfs->priv = (u64)fs;

    if (sb->s_rev_level == EXT2_GOOD_OLD_REV) {
        memcpy(&fs->sb.sb, sb, sizeof(*sb));
        fs->isdyn = 0;
    } else if (sb->s_rev_level == EXT2_DYNAMIC_REV) {
        ext2_dynrev_sb_t* dynsb = (ext2_dynrev_sb_t*)rawsb;
        memcpy(&fs->sb, dynsb, sizeof(*dynsb));
        if (dynsb->s_feature_incompat & ~(EXT2_FEATURE_INCOMPAT_FILETYPE)) {
            free(fs);
            return -EINVAL;
        }
        
        if (dynsb->s_feature_ro_compat & ~(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER | EXT2_FEATURE_RO_COMPAT_LARGE_FILE)) {
            free(fs);
            return -EINVAL;
        }

        fs->isdyn = 1;
    } else {
        free(fs);
        return -EINVAL;
    }

    fs->blocksz = 1024 << sb->s_log_block_size;
    fs->spb = 1024 / 512;
    fs->ppb = fs->blocksz / sizeof(u32);
    fs->gpb = fs->blocksz / sizeof(ext2_bg_t);

    fs->nbgs = sb->s_blocks_count / sb->s_blocks_per_group;
    if (sb->s_blocks_count % sb->s_blocks_per_group != 0)  fs->nbgs++;

    fs->bgtbln = (fs->nbgs * sizeof(*fs->bgs)) / fs->blocksz;
    if ((fs->nbgs * sizeof(*fs->bgs)) % fs->blocksz != 0) fs->bgtbln++;

    fs->bgs = malloc(sizeof(*fs->bgs) * fs->nbgs);
    if (!fs->bgs) {
        free(fs);
        return -ENOMEM;
    }

    u8 rawdscblk[fs->blocksz];
    u32 nbgsp = 0;
    ext2_bg_t* ptr = fs->bgs;
    u32 cblk = sb->s_first_data_block + 1;
    for (usize i = 0; i < fs->bgtbln; i++) {
        if ((ret = rdblk(vfs, cblk, rawdscblk)) < 0) return ret;

        u8 dscib = fs->gpb;
        if (nbgsp + dscib > fs->nbgs) {
            dscib = fs->nbgs - nbgsp;
        }

        memcpy(ptr, rawdscblk, dscib * sizeof(*fs->bgs));
        ptr += dscib;
        nbgsp += dscib;
        cblk++;
    }

    vfs->root_ino = EXT2_ROOT_INO;
    if ((ret = ext2fs_mount_setops(vfs)) < 0) {
        free(fs->bgs);
        free(fs);
        return ret;
    }

    return 0;
}