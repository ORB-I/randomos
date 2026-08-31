#include <core/std.h>
#include <core/lock.h>
#include <core/liballoc.h>
#include <core/printf.h>
#include <core/fd.h>
#include <lib/string.h>
#include <drivers/storage/fs.h>
#include <drivers/storage/ext2.h>
#include <drivers/storage/block.h>
#include <drivers/time/gettimeofday.h>
#include <scheduler/process.h>

#define CWD_MAX 1024
static char ext2_cwd[CWD_MAX+1];
static lock_t ext2_cwdlk = {0};

u32 cwdino  = EXT2_BAD_INO;
u32 pcwdino = EXT2_BAD_INO;

int ext2_ismnt = 0;
int ext2_isdyn = 0;
int ext2_ftdir = 0;

ext2_dynrev_sb_t ext2_sb;

u32 ext2_nbgs = 0;
u32 ext2_bgtbln = 0;
ext2_bg_t* ext2_bgs = NULL;

// TODO: add support for different block sizes
static int rdblk(u32 blk, u8* out) {
    if (blk >= ext2_sb.sb.s_blocks_count) return -1;
    if (block_read(0, out, blk * 2, 2) != 0) return -1;
    return 0;
}

static int wrblk(u32 blk, const u8* in) {
    if (blk >= ext2_sb.sb.s_blocks_count) return -1;
    if (block_write(0, in, blk * 2, 2) != 0) return -1;
    return 0;
}

#define BMP_SET 1
#define BMP_CLR 2
#define BMP_GET 3
static int modbmp(usize blkid, usize idx, int op) {
    u8 bmp[1024];

    if (rdblk(blkid, bmp) < 0) return -1;

    if (op == BMP_SET) {
        bmp[idx / 8] |= (1 << (idx % 8));
    } else if (op == BMP_CLR) {
        bmp[idx / 8] &= ~(1 << (idx % 8));
    } else if (op == BMP_GET) {
        return (bmp[idx / 8] & (1 << (idx % 8)));
    } else {
        return -1;
    }

    if (wrblk(blkid, bmp) < 0) return -1;
    return 0;
}

static int inodeused(u32 ino) {
    int res = modbmp(ext2_bgs[(ino - 1) / ext2_sb.sb.s_inodes_per_group].bg_inode_bitmap, (ino - 1) % ext2_sb.sb.s_inodes_per_group, BMP_GET);
    if (res < 0) return -1;
    else return res != 0;
}

static int ispow(u32 n, u32 base) {
    if (n == 0) return 0;
    while (n % base == 0) {
        n /= base;
    }
    return n == 1;
}

static int has_sparse(u32 bg) {
    if (ext2_sb.sb.s_rev_level == EXT2_GOOD_OLD_REV || ext2_sb.s_feature_ro_compat &~ EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER) {
        return 1;
    } else {
        if (bg == 0 || bg == 1) {
            return 1;
        }

        if (ispow(bg, 3) || ispow(bg, 5) || ispow(bg, 7)) {
            return 1;
        }
    }

    return 0;
}

static int flush_sbs() {
    for (usize i = 0; i < ext2_nbgs; i++) {
        if (has_sparse(i)) {
            u32 blkid = ext2_sb.sb.s_first_data_block + i * ext2_sb.sb.s_blocks_per_group;
            u8 buf[1024] = {0};
            if (ext2_sb.sb.s_rev_level == EXT2_GOOD_OLD_REV) {
                memcpy(buf, &ext2_sb.sb, sizeof(ext2_sb.sb));
            } else {
                memcpy(buf, &ext2_sb.sb, sizeof(ext2_sb.sb));
            }
            if (wrblk(blkid, buf) < 0) return -1;
        }
    }

    return 0;
}

static int flush_bgs() {
    for (usize grp = 0; grp < ext2_nbgs; grp++) {
        if (has_sparse(grp)) {
            u32 grpst = ext2_sb.sb.s_first_data_block + grp * ext2_sb.sb.s_blocks_per_group;
            u32 bgtst = grpst + 1;

            usize gpb = 1024 / sizeof(ext2_bg_t);
            for (usize blk = 0; blk < ext2_bgtbln; blk++) {
                usize fst = blk * gpb;
                if (fst >= ext2_nbgs) break;

                usize cnt = ext2_nbgs - fst;
                if (cnt > gpb) cnt = gpb;
                u8 buf[1024] = {0};
                memcpy(buf, &ext2_bgs[fst], cnt * sizeof(ext2_bg_t));
                if (wrblk(bgtst + blk, buf) < 0) return -1;
            }
        }
    }

    return 0;
}

static int flush_inode(u64 ino, ext2_ino_t* inod) {
    usize inoidx = (ino - 1) % ext2_sb.sb.s_inodes_per_group;
    usize inoblk = ext2_bgs[(ino - 1) / ext2_sb.sb.s_inodes_per_group].bg_inode_table + (inoidx / 8);

    ext2_ino_t inos[8];
    if (rdblk(inoblk, (u8*)inos) != 0) return -1;

    memcpy(&inos[inoidx % 8], inod, sizeof(*inod));
    if (wrblk(inoblk, (u8*)inos) != 0) return -1;

    return 0;
}

static int read_indr(u32 blkn, u32* obuf) {
    if (blkn == 0) {
        memset(obuf, 0, 1024);
        return 0;
    }
    return rdblk(blkn, (u8*)obuf);
}

static ssize ino_getblkid(ext2_ino_t* inod, usize idx) {
    if (!inod) return -1;

    const u32 ppb = 1024 / 4;
    u32 indrs[3][ppb];

    if (idx < 12) {
        return inod->i_block[idx];
    } else if (idx < 12 + ppb) {
        if (read_indr(inod->i_block[12], indrs[0]) != 0) return -1;
        return indrs[0][idx-12];
    } else if (idx < 12 + ppb + (ppb * ppb)) {
        u32 ridx = idx - 12 - ppb;
        if (read_indr(inod->i_block[13], indrs[0]) != 0) return -1;
        if (read_indr(indrs[0][ridx / ppb], indrs[1]) != 0) return -1;
        return indrs[1][ridx % ppb];
    } else {
        u32 ridx = idx - 12 - ppb - (ppb * ppb);

        if (read_indr(inod->i_block[14], indrs[0]) != 0) return -1;
        if (read_indr(indrs[0][ridx / (ppb * ppb)], indrs[1]) != 0) return -1;
        if (read_indr(indrs[1][(ridx / ppb) % ppb], indrs[2]) != 0) return -1;

        return indrs[2][ridx % ppb];
    }
}

static int membeq(void* ptr, u8 b, usize sz) {
    for (usize i = 0; i < sz; i++) {
        if (((char*)ptr)[i] != b) return 0;
    }
    return 1;
}

static ssize allocblk(u32 bg) {
    if (bg >= ext2_nbgs) return -1;
    for (usize n = 0; n < ext2_nbgs; n++) {
        u32 grp = (bg + n) % ext2_nbgs;
        u8 bmp[1024];
        if (ext2_bgs[grp].bg_free_blocks_count == 0) continue;
        if (rdblk(ext2_bgs[grp].bg_block_bitmap, bmp) < 0) return -1;
    
        for (usize i = 0; i < 1024; i++) {
            for (usize b = 0; b < 8; b++) {
                if (!(bmp[i] & (1 << b))) {
                    usize idx = i * 8 + b;
                    bmp[i] |= (1 << b);
                    if (wrblk(ext2_bgs[grp].bg_block_bitmap, bmp) < 0) return -1;

                    ext2_bgs[grp].bg_free_blocks_count--;
                    ext2_sb.sb.s_free_blocks_count--;
                    
                    flush_bgs();
                    flush_sbs();
                    return ext2_sb.sb.s_first_data_block + grp * ext2_sb.sb.s_blocks_per_group + idx;
                }
            }
        }
    }
    return -1;
}

static ssize ino_allocblk(u32 ino, ext2_ino_t* inod) {
    u32 inobg = (ino - 1) / ext2_sb.sb.s_inodes_per_group;
    for (usize i = 0; i < 12; i++) {
        if (inod->i_block[i] == 0) {
            ssize ret = allocblk(inobg);
            if (ret < 0) return -1;
            inod->i_block[i] = ret;
            inod->i_blocks += 2;
            if (flush_inode(ino, inod) < 0) return -1;
            return ret;
        }
    }

    const u32 ppb = 1024 / 4;
    u32 indrs[3][ppb];

    if (inod->i_block[12] == 0) {
        ssize ret = allocblk(inobg);
        if (ret < 0) return -1;
        inod->i_block[12] = ret;
        inod->i_blocks += 2;
        if (flush_inode(ino, inod) < 0) return -1;
    }

    if (read_indr(inod->i_block[12], indrs[0]) != 0) return -1;
    for (usize i = 0; i < ppb; i++) {
        if (indrs[0][i] == 0) {
            ssize ret = allocblk(inobg);
            if (ret < 0) return -1;
            indrs[0][i] = ret;
            inod->i_blocks += 2;
            if (wrblk(inod->i_block[12], (u8*)indrs[0]) < 0) return -1;
            if (flush_inode(ino, inod) < 0) return -1;
            return ret;
        }
    }

    if (inod->i_block[13] == 0) {
        ssize ret = allocblk(inobg);
        if (ret < 0) return -1;
        inod->i_block[13] = ret;
        inod->i_blocks += 2;
        if (flush_inode(ino, inod) < 0) return -1;
    }

    if (read_indr(inod->i_block[13], indrs[0]) != 0) return -1;
    for (usize i = 0; i < ppb; i++) {
        if (indrs[0][i] == 0) {
            ssize ret = allocblk(inobg);
            if (ret < 0) return -1;
            indrs[0][i] = ret;
            inod->i_blocks += 2;
            if (wrblk(inod->i_block[13], (u8*)indrs[0]) < 0) return -1;
            if (flush_inode(ino, inod) < 0) return -1;
        }

        if (read_indr(indrs[0][i], indrs[1]) < 0) return -1;
        for (usize j = 0; j < ppb; j++) {
            if (indrs[1][j] == 0) {
                ssize ret = allocblk(inobg);
                if (ret < 0) return -1;
                indrs[1][j] = ret;
                inod->i_blocks += 2;
                if (wrblk(indrs[0][i], (u8*)indrs[1]) < 0) return -1;
                if (flush_inode(ino, inod) < 0) return -1;
                return ret;
            }
        }
    }

    if (inod->i_block[14] == 0) {
        ssize ret = allocblk(inobg);
        if (ret < 0) return -1;
        inod->i_block[14] = ret;
        inod->i_blocks += 2;
        if (flush_inode(ino, inod) < 0) return -1;
    }

    if (read_indr(inod->i_block[14], indrs[0]) != 0) return -1;
    for (usize i = 0; i < ppb; i++) {
        if (indrs[0][i] == 0) {
            ssize ret = allocblk(inobg);
            if (ret < 0) return -1;
            indrs[0][i] = ret;
            inod->i_blocks += 2;
            if (wrblk(inod->i_block[14], (u8*)indrs[0]) < 0) return -1;
            if (flush_inode(ino, inod) < 0) return -1;
        }

        if (read_indr(indrs[0][i], indrs[1]) < 0) return -1;
        for (usize j = 0; j < ppb; j++) {
            if (indrs[1][j] == 0) {
                ssize ret = allocblk(inobg);
                if (ret < 0) return -1;
                indrs[1][j] = ret;
                inod->i_blocks += 2;
                if (wrblk(indrs[0][j], (u8*)indrs[1]) < 0) return -1;
                if (flush_inode(ino, inod) < 0) return -1;
            }

            if (read_indr(indrs[1][j], indrs[2]) < 0) return -1;
            for (usize k = 0; k < ppb; k++) {
                if (indrs[2][k] == 0) {
                    ssize ret = allocblk(inobg);
                    if (ret < 0) return -1;
                    indrs[2][k] = ret;
                    inod->i_blocks += 2;
                    if (wrblk(indrs[1][j], (u8*)indrs[2]) < 0) return -1;
                    if (flush_inode(ino, inod) < 0) return -1;
                    return ret;
                }
            }
        }
    }
}

static int freeblk(u32 blkid) {
    u32 bg = (blkid - ext2_sb.sb.s_first_data_block) /  ext2_sb.sb.s_blocks_per_group;
    u32 bmpidx = (blkid - ext2_sb.sb.s_first_data_block) % ext2_sb.sb.s_blocks_per_group;

    u8 zero[1024] = {0};
    wrblk(blkid, zero);

    if (modbmp(ext2_bgs[bg].bg_block_bitmap, bmpidx, BMP_CLR) < 0) return -1;
    ext2_bgs[bg].bg_free_blocks_count++;
    ext2_sb.sb.s_free_blocks_count++;

    if (flush_bgs() < 0) return -1;
    if (flush_sbs() < 0) return -1;
}

static int ino_freeblk(u32 ino, ext2_ino_t* inod, usize idx) {
    const u32 ppb = 1024 / 4;
    u32 indrs[3][ppb];

    if (idx < 12) {
        if (freeblk(inod->i_block[idx]) < 0) return -1;
        inod->i_block[idx] = 0;
        inod->i_blocks--;
        if (flush_inode(ino, inod) < 0) return -1;
    } else if (idx < 12 + ppb) {
        if (read_indr(inod->i_block[12], indrs[0]) != 0) return -1;
        usize pblk = indrs[0][idx-12];

        if (freeblk(pblk) < 0) return -1;
        indrs[0][idx-12] = 0;
        inod->i_blocks--;

        if (membeq(indrs[0], 0, 1024)) {
            if (freeblk(inod->i_block[12]) < 0) return -1;
            inod->i_block[12] = 0;
            inod->i_blocks--;
        } else {
            if (wrblk(inod->i_block[12], (u8*)indrs[0]) < 0) return -1;
        }

        if (flush_inode(ino, inod) < 0) return -1;
    } else if (idx < 12 + ppb + (ppb * ppb)) {
        u32 ridx = idx - 12 - ppb;
        if (read_indr(inod->i_block[13], indrs[0]) != 0) return -1;
        if (read_indr(indrs[0][ridx / ppb], indrs[1]) != 0) return -1;

        usize pblk = indrs[1][ridx % ppb];
        if (freeblk(pblk) < 0) return -1;
        indrs[1][ridx % ppb] = 0;
        inod->i_blocks--;

        if (membeq(indrs[1], 0, 1024)) {
            if (freeblk(indrs[0][ridx / ppb]) < 0) return -1;
            indrs[0][ridx / ppb] = 0;
            inod->i_blocks--;

            if (membeq(indrs[0], 0, 1024)) {
                if (freeblk(inod->i_block[13]) < 0) return -1;
                inod->i_block[13] = 0;
                inod->i_blocks--;
            } else {
                if (wrblk(inod->i_block[13], (u8*)indrs[0]) < 0) return -1;
            }
        } else {
            if (wrblk(indrs[0][ridx / ppb], (u8*)indrs[1]) < 0) return -1;
        }

        if (flush_inode(ino, inod) < 0) return -1;
    } else {
        u32 ridx = idx - 12 - ppb - (ppb * ppb);

        if (read_indr(inod->i_block[14], indrs[0]) != 0) return -1;
        if (read_indr(indrs[0][ridx / (ppb * ppb)], indrs[1]) != 0) return -1;
        if (read_indr(indrs[1][(ridx / ppb) % ppb], indrs[2]) != 0) return -1;

        usize pblk = indrs[2][ridx % ppb];
        if (freeblk(pblk) < 0) return -1;
        indrs[2][ridx % ppb] = 0;
        inod->i_blocks--;

        if (membeq(indrs[2], 0, 1024)) {
            if (freeblk(indrs[1][(ridx / ppb) % ppb]) < 0) return -1;
            indrs[1][(ridx / ppb) % ppb] = 0;
            inod->i_blocks--;

            if (membeq(indrs[1], 0, 1024)) {
                if (freeblk(indrs[0][ridx / (ppb * ppb)]) < 0) return -1;
                indrs[0][ridx / (ppb * ppb)] = 0;
                inod->i_blocks--;

                if (membeq(indrs[0], 0, 1024)) {
                    if (freeblk(inod->i_block[14]) < 0) return -1;
                    inod->i_block[14] = 0;
                    inod->i_blocks--;
                } else {
                    if (wrblk(inod->i_block[14], (u8*)indrs[0]) < 0) return -1;
                }
            } else {
                if (wrblk(indrs[0][ridx / (ppb * ppb)], (u8*)indrs[1]) < 0) return -1;
            }
        } else {
            if (wrblk(indrs[1][(ridx / ppb) % ppb], (u8*)indrs[2]) < 0) return -1;
        }

        if (flush_inode(ino, inod) < 0) return -1;
    }
}

static u8* getinodata(ext2_ino_t* inode, usize* outsz) {
    if (!inode || !outsz) return NULL;

    u32 tsz = inode->i_size;
    *outsz = tsz;
    if (tsz == 0) return NULL;

    u8* obuf = malloc(tsz);
    if (!obuf) return NULL;

    u32 tblks = (tsz + 1024 - ext2_sb.sb.s_first_data_block) / 1024;

    u32 bidx = 0;
    while (bidx < tblks) {
        ssize pblk = ino_getblkid(inode, bidx++);
        if (pblk < 0) goto err;

        usize blft = tsz - ((bidx - 1) * 1024);
        usize b2cp = (blft > 1024) ? 1024 : blft;
        u8* dptr = obuf + ((bidx - 1) * 1024);

        if (pblk == 0) {
            memset(dptr, 0, b2cp);
        } else if (b2cp == 1024) {
            if (rdblk(pblk, dptr) != 0) goto err;
        } else {
            u8 tmp[1024];
            if (rdblk(pblk, tmp) != 0) goto err;
            memcpy(dptr, tmp, b2cp);
        }
    }

    return obuf;
err:
    *outsz = 0;
    free(obuf);
    return NULL;
}

static int getino(u32 ino, ext2_ino_t* inod) {
    if (!inod) return -1;

    u32 inobg = (ino - 1) / ext2_sb.sb.s_inodes_per_group;
    ext2_bg_t* bg = &ext2_bgs[inobg];

    if (!inodeused(ino)) return -1;

    usize inoidx = (ino - ext2_sb.sb.s_first_data_block) % ext2_sb.sb.s_inodes_per_group;
    usize inoblk = bg->bg_inode_table + (inoidx / 8);

    ext2_ino_t inos[8];
    if (rdblk(inoblk, (u8*)inos) != 0) return -1;

    memcpy(inod, &inos[inoidx % 8], sizeof(*inod));
    return 0;
}

static u32 searchdir(u32 dino, const char* name, ext2_ino_t* ino) {
    ext2_ino_t dinod;
    if (getino(dino, &dinod) < 0) return -1;

    usize dirsz = 0;
    u8* ddata = getinodata(&dinod, &dirsz);
    if (!ddata) return EXT2_BAD_INO;

    for (usize i = 0; i < dirsz;) {
        // we can use this even though in dir0_t namelen is u16
        // because you can only have up to 255 chars for name or UINT8_MAX
        ext2_dir1_t* dir = (ext2_dir1_t*)&ddata[i];
        if (dir->name_len == strlen(name) && memcmp(dir->name, name, dir->name_len) == 0) {
            if (getino(dir->inode, ino) < 0) return EXT2_BAD_INO;
            free(ddata);
            return dir->inode;
        }

        i += dir->rec_len;
    }

    free(ddata);
    return EXT2_BAD_INO;
}

static u32 findino(const char* path, ext2_ino_t* inod) {
    u32 ino;
    if (!path || *path == '\0') return EXT2_BAD_INO;

    if (path[0] == '/') {
        ino = EXT2_ROOT_INO;
        path++;

        if (*path == '\0') {
            if (getino(ino, inod) < 0) return EXT2_BAD_INO;
            return ino;
        }
    } else {
        ino = cwdino;
    }

    while (*path) {
        char comp[256];
        usize len = 0;

        while (*path == '/') path++;
        if (*path == '\0') break;

        while (path[len] != '/' && path[len] != '\0') {
            if (len >= sizeof(comp) - 1) return EXT2_BAD_INO;
            comp[len] = path[len];
            len++;
        }

        comp[len] = '\0';
        path += len;

        if (streq(comp, ".")) continue;
        if (streq(comp, "..")) {
            ext2_ino_t inod;
            ino = searchdir(ino, "..", &inod);
            if (ino == EXT2_BAD_INO) return ino;
            continue;
        }

        ext2_ino_t nxt;
        if ((ino = searchdir(ino, comp, &nxt)) == EXT2_BAD_INO) return EXT2_BAD_INO;
    }

    if (getino(ino, inod) < 0) return EXT2_BAD_INO;
    return ino;
}

static ssize newino(u32 bg, u16 mode, u16 uid, u16 gid) {
    if (bg >= ext2_nbgs) return -1;
    for (usize n = 0; n < ext2_nbgs; n++) {
        u32 grp = (bg + n) % ext2_nbgs;
        u8 bmp[1024];
        if (ext2_bgs[grp].bg_free_inodes_count == 0) continue;
        if (rdblk(ext2_bgs[grp].bg_inode_bitmap, bmp) < 0) return -1;

        for (usize i = 0; i < 1024; i++) {
            for (usize b = 0; b < 8; b++) {
                if (!(bmp[i] & (1 << b))) {
                    usize idx = i * 8 + b;
                    bmp[i] |= (1 << b);
                    if (wrblk(ext2_bgs[grp].bg_inode_bitmap, bmp) < 0) return -1;

                    ext2_bgs[grp].bg_free_inodes_count--;
                    ext2_sb.sb.s_free_inodes_count--;

                    flush_bgs();
                    flush_sbs();

                    u64 time = gettimeofday();

                    ext2_ino_t inode = {
                        mode, uid, 0,
                        time, time, time, 0,
                        gid, 0, 0, 0,
                        0, {0}, 0, 0,
                        0, 0, {0}
                    };

                    u32 ino = grp * ext2_sb.sb.s_inodes_per_group + idx;
                    if (flush_inode(ino, &inode) < 0) return -1;

                    return ino;
                }
            }
        }
    }

    return -1;
}

static int path_nameprts(const char* path, char* base, usize baselen, char* dir, usize dirlen) {
    usize len = strlen(path);
    while (len > 0 && path[len - 1] == '/') len--;

    if (len == 0) return -1;

    usize slash = len;
    while (slash > 0 && path[slash - 1] != '/') slash--;

    char* fpath = strcpy(path);
    if (!fpath) return -1;

    char* bname = &fpath[slash];
    usize bnamlen = len - slash;

    fpath[slash] = '\0';
    char* parname = fpath;
    usize parlen = strlen(parname);

    if (dirlen > parlen + 1) {
        memcpy(dir, parname, parlen + 1);
    } else {
        memcpy(dir, parname, dirlen-1);
        dir[dirlen] = '\0';
    }

    if (baselen > bnamlen + 1) {
        memcpy(base, bname, bnamlen + 1);
    } else {
        memcpy(base, bname, baselen-1);
        base[baselen] = '\0';
    }

    free(fpath);
    return 0;
}

static int rmlink(const char* path) {
    char dir[1024], name[1024];
    if (path_nameprts(path, name, 1024, dir, 1024) < 0) return -1;
    ext2_ino_t dinod;
    u32 dino = findino(dir, &dinod);
    if (dino == EXT2_BAD_INO) return -1;

    usize nblks = (dinod.i_size + 1023) / 1024;
    for (usize b = 0; b < nblks; b++) {
        ssize blk = ino_getblkid(&dinod, b);
        if (blk < 0) {
            return -1;
        }

        u8 blkd[1024];
        if (rdblk(blk, blkd) < 0) return -1;
        ext2_dir1_t* pdir = NULL;
        for (usize i = 0; i < 1024;) {
            ext2_dir1_t* dir = (ext2_dir1_t*)&blkd[i];
            if (dir->name_len == strlen(name) && memcmp(dir->name, name, dir->name_len) == 0) {
                pdir->rec_len += dir->rec_len;
                memset(dir, 0, dir->rec_len);
                if (wrblk(blk, blkd) < 0) return -1;
                return 0;
            }
            pdir = dir;
            i += dir->rec_len;
        }
    }

    return -1;
}

static int mklink(const char* path, u32 ino, u16 mode) {
    char dir[1024], base[1204];
    if (path_nameprts(path, base, 1024, dir, 1024) < 0) return -1;

    ext2_ino_t parinod;
    u32 parino = findino(dir, &parinod);
    if (parino == EXT2_BAD_INO) return -1;

    usize newsz = EXT2_DIR_RECLEN(strlen(base));
    usize nblks = (parinod.i_size + 1023) / 1024;

    ext2_dir1_t newent = {
        ino, 0, strlen(base), 0, {0}
    };
    memcpy(newent.name, base, strlen(base));

    if (ext2_sb.sb.s_rev_level == EXT2_DYNAMIC_REV && ext2_sb.s_feature_incompat & EXT2_FEATURE_INCOMPAT_FILETYPE)  {
        if (mode & EXT2_S_IFSOCK) {
            newent.file_type = EXT2_FT_SOCK;
        } else if (mode & EXT2_S_IFLNK) {
            newent.file_type = EXT2_FT_SYMLINK;
        } else if (mode & EXT2_S_IFREG) {
            newent.file_type = EXT2_FT_REG_FILE;
        } else if (mode & EXT2_S_IFBLK) {
            newent.file_type = EXT2_FT_BLKDEV;
        } else if (mode & EXT2_S_IFDIR) {
            newent.file_type = EXT2_FT_DIR;
        } else if (mode & EXT2_S_IFCHR) {
            newent.file_type = EXT2_FT_CHRDEV;
        } else if (mode & EXT2_S_IFIFO) {
            newent.file_type = EXT2_FT_FIFO;
        } else {
            newent.file_type = EXT2_FT_UNKNOWN;
        }
    }

    for (usize b = 0; b < nblks; b++) {
        ssize blk = ino_getblkid(&parinod, b);
        if (blk < 0) return -1;

        u8 blkd[1024];
        if (rdblk(blk, blkd) < 0) return -1;
        for (usize i = 0; i < 1024;) {
            ext2_dir1_t* ent = (ext2_dir1_t*)&blkd[i];
            if (ent->rec_len < 8 || i + ent->rec_len > 1024) return -1;
            usize oldsz = EXT2_DIR_RECLEN(ent->name_len);

            if (ent->inode == 0) {
                if (ent->rec_len >= newsz) {
                    newent.rec_len = ent->rec_len;
                    memcpy(&blkd[i], &newent, newsz);

                    if (wrblk(blk, blkd) < 0) return -1;
                    return 0;
                }
            } else if (ent->rec_len >= oldsz && ent->rec_len - oldsz >= newsz) {
                newent.rec_len = ent->rec_len - oldsz;
                ent->rec_len = oldsz;
                memcpy(&blkd[i + oldsz], &newent, newsz);

                if (wrblk(blk, blkd) < 0) return -1;
                return 0;
            }

            i += ent->rec_len;
        }
    }

    ssize nblk = ino_allocblk(parino, &parinod);
    if (nblk < 0) return -1;

    u8 blkd[1024] = {0};
    newent.rec_len = 1024;
    memcpy(blkd, &newent, EXT2_DIR_RECLEN(strlen(base)));
    
    if (wrblk(nblk, blkd) < 0) return -1;
    return 0;
}

#define CONVE2(NAME) (e2mode & EXT2_##NAME) { mode |= NAME; }
static u32 e2mode_conv(u32 e2mode) {
    u32 mode = 0;

    if CONVE2(S_IXOTH) 
    else if CONVE2(S_IWOTH) 
    else if CONVE2(S_IROTH)

    else if CONVE2(S_IXGRP)
    else if CONVE2(S_IWGRP)
    else if CONVE2(S_IRGRP)

    else if CONVE2(S_IXUSR)
    else if CONVE2(S_IWUSR)
    else if CONVE2(S_IRUSR)

    else if CONVE2(S_ISVTX)
    else if CONVE2(S_ISGID)
    else if CONVE2(S_ISUID)

    else if CONVE2(S_IFIFO)
    else if CONVE2(S_IFCHR)
    else if CONVE2(S_IFDIR)
    else if CONVE2(S_IFBLK)
    else if CONVE2(S_IFREG)
    else if CONVE2(S_IFLNK)
    else if CONVE2(S_IFSOCK)

    return mode;
}

#define CONVSYS(NAME) (sysmode & NAME) { mode |= EXT2_##NAME; }
static u32 sysmode_conv(u32 sysmode) {
    u32 mode = 0;

    if CONVSYS(S_IXOTH) 
    else if CONVSYS(S_IWOTH) 
    else if CONVSYS(S_IROTH)

    else if CONVSYS(S_IXGRP)
    else if CONVSYS(S_IWGRP)
    else if CONVSYS(S_IRGRP)

    else if CONVSYS(S_IXUSR)
    else if CONVSYS(S_IWUSR)
    else if CONVSYS(S_IRUSR)

    else if CONVSYS(S_ISVTX)
    else if CONVSYS(S_ISGID)
    else if CONVSYS(S_ISUID)

    else if CONVSYS(S_IFIFO)
    else if CONVSYS(S_IFCHR)
    else if CONVSYS(S_IFDIR)
    else if CONVSYS(S_IFBLK)
    else if CONVSYS(S_IFREG)
    else if CONVSYS(S_IFLNK)
    else if CONVSYS(S_IFSOCK)

    return mode;
}

int _ext2_mount(const char* path) {
    if (strlen(path) > CWD_MAX) return -1;
    if (streq(path, "") || streq(path, " ")) {
        memcpy(ext2_cwd, "/", 2);
    }

    u8 rawsb[1024];
    if (block_read(0, rawsb, 2, 2) != 0) return -1;
    ext2_sb_t* sb = (ext2_sb_t*)rawsb;

    if (sb->s_magic != EXT2_SUPER_MAGIC) return -1;

    if (sb->s_errors != EXT2_ERRORS_CONTINUE) return -1;

    ext2_nbgs = ext2_sb.sb.s_blocks_count / ext2_sb.sb.s_blocks_per_group;
    if (ext2_sb.sb.s_blocks_count % ext2_sb.sb.s_blocks_per_group != 0) ext2_nbgs++;

    ext2_bgtbln = (ext2_nbgs * sizeof(*ext2_bgs)) / 1024;
    if ((ext2_nbgs * sizeof(*ext2_bgs)) % 1024 != 0) ext2_bgtbln++;

    ext2_bgs = malloc(sizeof(*ext2_bgs) * ext2_nbgs);
    if (!ext2_bgs) return -1;

    u8 rawdscblk[1024];
    u32 nbgsp = 0;
    ext2_bg_t* ptr = ext2_bgs;
    u32 cblk = sb->s_first_data_block + 1;
    for (usize i = 0; i < ext2_bgtbln; i++) {
        if (rdblk(cblk, rawdscblk) != 0) return -1;

        u32 descs_ib = 1024 / sizeof(*ext2_bgs);
        if (nbgsp + descs_ib > ext2_nbgs) {
            descs_ib = ext2_nbgs - nbgsp;
        }

        memcpy(ptr, rawdscblk, descs_ib * sizeof(*ext2_bgs));
        ptr += descs_ib;
        nbgsp += descs_ib;
        cblk++;
    }

    if (sb->s_rev_level == EXT2_GOOD_OLD_REV) {
        memcpy(&ext2_sb.sb, sb, sizeof(*sb));
        ext2_isdyn = 0;
        ext2_ismnt = 1;
    } else if (sb->s_rev_level == EXT2_DYNAMIC_REV) {
        ext2_dynrev_sb_t* dynsb = (ext2_dynrev_sb_t*)rawsb;
        memcpy(&ext2_sb, dynsb, sizeof(*dynsb));

        if (dynsb->s_feature_incompat & ~(EXT2_FEATURE_INCOMPAT_FILETYPE)) {
            free(ext2_bgs);
            return -1;
        }
        
        if (dynsb->s_feature_ro_compat & ~(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)) {
            free(ext2_bgs);
            return -1;
        }

        ext2_isdyn = 1;
        ext2_ismnt = 1;
    } else {
        free(ext2_bgs);
        return -1;
    }
    
    lock_acquire(&ext2_cwdlk);
    memcpy(ext2_cwd, path, strlen(path)+1);
    lock_release(&ext2_cwdlk);

    return 0;
}

int _ext2_unmount() {
    ext2_ismnt = 0;
    ext2_isdyn = 0;

    lock_acquire(&ext2_cwdlk);
    memset(ext2_cwd, 0, sizeof(ext2_cwd));
    lock_release(&ext2_cwdlk);

    memset(&ext2_sb, 0, sizeof(ext2_sb));

    return 0;
}

int _ext2_trunc(int fd) {
    struct fdinfo* fdinfo;
    if (getfd(fd, &fdinfo) < 0) return -1;

    if (fdinfo->type != FDTYPE_E2ENT) return -1;
    struct ext2_entry* ent = &fdinfo->data.e2ent;

    for (usize i = 0; i < ent->inod.i_blocks / 2; i++) {
        if (ino_freeblk(ent->ino, &ent->inod, i) < 0) return -1;
    }
}

int _ext2_creat(const char* path, u16 mode) {
    ext2_ino_t inod;
    if (findino(path, &inod) != EXT2_BAD_INO) {
        return -1;
    }

    u32 e2mod = sysmode_conv(mode);

    u64 ino = 0;
    if ((ino = newino(0, e2mod, proctbl[current_pid].euid, proctbl[current_pid].egid)) < 0) return -1;

    return mklink(path, ino, e2mod);
}

int _ext2_open(const char* path, int flags, u16 mode) {
    ext2_ino_t inod;
    u32 ino = findino(path, &inod);
    if (ino == EXT2_BAD_INO) {
        if (flags & O_CREAT) {
            if (_ext2_creat(path, mode) < 0) {
                return -1;
            } else {
                ino = findino(path, &inod);
            }
        } else {
            return -1;
        }
    }

    struct fdinfo info = {
        0, 0, FDTYPE_E2ENT, {.e2ent = {
            inod, ino, 0, flags
        }}
    };

    if (flags & O_APPEND) {
        info.data.e2ent.pos = inod.i_size;
    }

    struct fdinfo* ninfo = NULL;
    if (!(ninfo = getnewfd(&info))) {
        return -1;
    }

    if (flags & O_TRUNC) {
        if (_ext2_trunc(ninfo->fd) < 0) {
            closefd(ninfo->fd);
            return -1;
        }
    }

    return ninfo->fd;
}

int _ext2_close(int fd) {
    closefd(fd);
}

ssize _ext2_read(int fd, void* buf, usize size) {
    struct fdinfo* info = NULL;
    if (getfd(fd, &info) < 0) return -1;

    if (info->type != FDTYPE_E2ENT || info->data.e2ent.inod.i_mode & EXT2_S_IFDIR) {
        return -1;
    }

    struct ext2_entry* ent = &info->data.e2ent;
    if (size > ent->inod.i_size - ent->pos) size = ent->inod.i_size - ent->pos;

    usize nread = 0;
    while (nread < size) {
        usize pos = ent->pos + nread;
        usize blk = pos / 1024;
        usize off = pos % 1024;

        ssize blkid = ino_getblkid(&ent->inod, blk);
        if (blkid < 0) return nread;

        u8 blkd[1024];
        if (rdblk(blkid, blkd) < 0) return -1;

        usize n = 1024 - off;
        if (n > size - nread) n = size - nread;
        memcpy((u8*)buf + nread, blkd + off, n);
        nread += n;
    }

    ent->pos += nread;
    return nread;
}

ssize _ext2_write(int fd, void* buf, usize size) {
    struct fdinfo* info = NULL;
    if (getfd(fd, &info) < 0) return -1;

    if (info->type != FDTYPE_E2ENT || info->data.e2ent.inod.i_mode & EXT2_S_IFDIR) {
        return -1;
    }

    struct ext2_entry* ent = &info->data.e2ent;
    
    usize nwriten = 0;
    while (nwriten < size) {
        usize pos = ent->pos + nwriten;
        usize blk = pos / 1024;
        usize off = pos % 1024;

        ssize blkid = ino_getblkid(&ent->inod, blk);
        if (blkid < 0) {
            if ((blkid = ino_allocblk(ent->ino, &ent->inod)) < 0) {
                return nwriten;
            }
        }

        u8 blkd[1024];
        if (rdblk(blkid, blkd) < 0) return -1;

        usize n = 1024 - off;
        if (n > size - nwriten) n = size - nwriten;
        memcpy(blkd + off, (u8*)buf + nwriten, n);
        if (wrblk(blkid, blkd) < 0) return -1;

        nwriten += n;
    }

    ent->pos += nwriten;
    return nwriten;
}

off_t _ext2_lseek(int fd, off_t off, int whence) {
    struct fdinfo* fdinfo;
    if (getfd(fd, &fdinfo) < 0) return -1;

    if (fdinfo->type != FDTYPE_E2ENT) return -1;
    struct ext2_entry* ent = &fdinfo->data.e2ent;

    if (whence == SEEK_SET) {
        if (off > ent->inod.i_size) return -1;
        ent->pos = off;
        return ent->pos;
    } else if (whence == SEEK_CUR) {
        if (ent->pos + off > ent->inod.i_size) return -1;
        ent->pos += off;
        return ent->pos;
    } else if (whence == SEEK_END) {
        if (off > 0) return -1;
        if (ent->inod.i_size + off < 0) return -1;
        ent->pos = ent->inod.i_size + off;
        return ent->pos;
    } else {
        return -1;
    }
}

// we sync automatically for now on any op
int _ext2_sync(int fd) { (void)fd; return 0; }

int _ext2_opendir(const char* path) {
    ext2_ino_t inod;
    u32 ino = findino(path, &inod);
    if (ino == EXT2_BAD_INO) {
        return -1;
    }

    if (!(inod.i_mode & EXT2_S_IFDIR)) {
        return -1;
    }

    struct fdinfo info = {
        0, 0, FDTYPE_E2ENT, {.e2ent = {
            inod, ino, 0, O_RDWR
        }}
    };

    struct fdinfo* ninfo = NULL;
    if (!(ninfo = getnewfd(&info))) {
        return -1;
    }

    return ninfo->fd;
}

int _ext2_closedir(int dd) {
    closefd(dd);
}

int _ext2_readdir(int dd, struct stat* st) {
    struct fdinfo* info;
    if (getfd(dd, &info) < 0) return -1;

    if (info->type != FDTYPE_E2ENT || !(info->data.e2ent.inod.i_mode & EXT2_S_IFDIR)) {
        return -1;
    }

    struct ext2_entry* ent = &info->data.e2ent;
    usize nblks = (ent->inod.i_size + 1023) / 1024;
    usize pos = 0;

    for (usize b = 0; b < nblks; b++) {
        ssize blk = ino_getblkid(&ent->inod, b);
        if (blk == EXT2_BAD_INO) return -1;

        u8 blkd[1024];
        if (rdblk(blk, blkd) < 0) return -1;
        for (usize i = 0; i < 1024;) {
            ext2_dir1_t* dir = (ext2_dir1_t*)&blkd[i];
            if (dir->rec_len < 8 || i + dir->rec_len > 1024) return -1;
            
            if (pos == ent->pos) {
                if (dir->inode == 0) {
                    return -1;
                } else {
                    ext2_ino_t inod;

                    if (getino(dir->inode, &inod) < 0) return -1;

                    st->mode = e2mode_conv(inod.i_mode);
                    st->st_attrib = 0;

                    memcpy(st->st_name, dir->name, dir->name_len);
                    st->st_name[dir->name_len] = '\0';

                    st->st_size = inod.i_size;

                    ent->pos++;
                    return 0;
                }
            }

            pos++;
            i += dir->rec_len;
        }
    }
    
    return -1;
}

int _ext2_stat(const char* path, struct stat* st) {
    ext2_ino_t inod;
    u32 ino = findino(path, &inod);
    if (ino == EXT2_BAD_INO) return -1;

    st->mode = e2mode_conv(inod.i_mode);
    st->st_attrib = 0;
    st->st_size = inod.i_size;

    usize len = strlen(path);
    while (len > 0 && path[len - 1] == '/') len--;

    if (len == 0) return -1;

    usize slash = len;
    while (slash > 0 && path[slash - 1] != '/') slash--;

    const char* bname = &path[slash];
    usize bnamlen = len - slash;

    memcpy(st->st_name, bname, bnamlen);

    return 0;
}

int _ext2_unlink(const char* path) {
    ext2_ino_t inod;
    u32 ino = findino(path, &inod);
    if (ino == EXT2_BAD_INO) return -1;

    char dir[1024], base[1024];
    if (path_nameprts(path, base, 1024, dir, 1024) < 0) return -1;

    if (inod.i_links_count > 1) {
        if (rmlink(path) < 0) return -1;
        inod.i_links_count--;
        return flush_inode(ino, &inod);
    } else {
        if (rmlink(path) < 0) return -1;
        // now since theres no more links were gonna like actually
        // destroy the data and link

        for (usize i = 0; i < inod.i_blocks / 2; i++) {
            if (ino_freeblk(ino, &inod, i) < 0) return -1;
        }

        memset(&inod, 0, sizeof(inod));
        
        u32 inobg = (ino - 1) / ext2_sb.sb.s_inodes_per_group;
        ext2_bg_t* bg = &ext2_bgs[inobg];

        bg->bg_free_inodes_count++;
        ext2_sb.sb.s_free_inodes_count++;
        if (modbmp(bg->bg_inode_bitmap, (ino - 1) % ext2_sb.sb.s_inodes_per_group, BMP_CLR) < 0) return -1;

        flush_inode(ino, &inod);
        flush_bgs();
        flush_sbs();
    }
    
}

int _ext2_rmdir(const char* path) {
    ext2_ino_t inod;
    u32 ino = findino(path, &inod);
    if (ino == EXT2_BAD_INO) return -1;

    char dir[1024], base[1024];
    if (path_nameprts(path, base, 1024, dir, 1024) < 0) return -1;

    usize dirsz = 0;
    u8* ddata = getinodata(&inod, &dirsz);
    if (!ddata) return EXT2_BAD_INO;

    for (usize i = 0; i < dirsz;) {
        ext2_dir1_t* dir = (ext2_dir1_t*)&ddata[i];
        if (!((dir->name_len == 1 && dir->name[0] == '.') || (dir->name_len == 2 && dir->name[0] == '.' && dir->name[1] == '.'))) {
            free(ddata);
            return -1;
        }
        i += dir->rec_len;
    }

    free(ddata);
    return _ext2_unlink(path);
}

int _ext2_rename(const char* oname, const char* nname) {
    ext2_ino_t inod;
    u32 ino = findino(oname, &inod);
    if (ino == EXT2_BAD_INO) return -1;

    if (mklink(nname, ino, inod.i_mode) < 0) return -1;
    return rmlink(oname);
}

int _ext2_mkdir(const char* path, u16 mode) {
    if (_ext2_creat(path, S_IFDIR | mode) < 0) return -1;

    char dir[1024], name[1024];
    if (path_nameprts(path, name, 1024, dir, 1024) < 0) return -1;

    ext2_ino_t dinod;
    u32 dino = findino(dir, &dinod);
    if (dino == EXT2_BAD_INO) {
        _ext2_unlink(path);
        return -1;
    }

    ext2_ino_t inod;
    u32 ino = findino(path, &inod);
    if (ino == EXT2_BAD_INO) {
        _ext2_unlink(path);
        return -1;
    }

    char dot[1024], dotdot[1024];
    int ret = snprintf(dot, 1024, "%s/.", path);
    if (ret >= 1024 || ret < 0) {
        _ext2_unlink(path);
        return -1;
    }

    ret = snprintf(dotdot, 1024, "%s/..", path);
    if (ret >= 1024 || ret < 0) {
        _ext2_unlink(path);
        return -1;
    }

    if (mklink(dot, ino, inod.i_mode) < 0) {
        _ext2_rmdir(path);
        return -1;
    }

    if (mklink(dotdot, dino, dinod.i_mode) < 0) {
        _ext2_rmdir(path);
        return -1;
    }

    return 0;
}

int _ext2_chdir(const char* path) {
    if (strlen(path) >= CWD_MAX) return -1;

    char dir[1024];
    if (path_nameprts(path, NULL, 0, dir, 1024) < 0) return -1;

    ext2_ino_t dinod;
    u32 dino = findino(dir, &dinod);
    if (dino == EXT2_BAD_INO) return -1;

    ext2_ino_t inod;
    u32 ino = findino(path, &inod);
    if (ino == EXT2_BAD_INO) return -1;

    lock_acquire(&ext2_cwdlk);
    memcpy(ext2_cwd, path, strlen(path)+1);
    cwdino = ino;
    pcwdino = dino;
    lock_release(&ext2_cwdlk);

    return 0;
}

int _ext2_getcwd(char* path, usize len) {
    lock_acquire(&ext2_cwdlk);
    usize cwdlen = strlen(ext2_cwd);
    if (len >= cwdlen) {
        memcpy(path, ext2_cwd, cwdlen+1);
    } else {
        memcpy(path, ext2_cwd, len-1);
        path[len] = '\0';
    }
    lock_release(&ext2_cwdlk);
    return 0;
}