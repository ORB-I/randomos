#include <core/std.h>
#include <core/liballoc.h>
#include <core/printf.h>
#include <core/fd.h>
#include <core/lock.h>
#include <drivers/display/serial.h>
#include <lib/string.h>
#include <drivers/storage/block.h>
#include <drivers/storage/old_ext2.h>

/* read-only ext2. nothing on the disk is trusted: every field gets
   range-checked before it can influence an allocation, a device read or
   a pointer. metadata that fails validation fails the syscall instead
   of corrupting memory */

#define EXT2_MAGIC 0xEF53
#define EXT2_ROOT_INO 2

#define E2_IFDIR  0x4000
#define E2_IFREG  0x8000
#define E2_IFLNK  0xA000

/* feature gates. incompat bits we dont understand mean on-disk shapes
   we cant reason about, so the image gets refused outright */
#define INCOMPAT_FILETYPE 0x0002
#define INCOMPAT_KNOWN    INCOMPAT_FILETYPE
/* ro-compat bits that dont change how we read: sparse_super, large_file */
#define ROCOMPAT_KNOWN    0x0003

#define NAME_MAX_LEN 255
#define CWD_MAX 256

/* superblock fields we care about, all parsed through le32/le16 */
#define SB_OFF_MAGIC        56
#define SB_OFF_REV          76
#define SB_OFF_INODE_SIZE   88
#define SB_OFF_INCOMPAT     96
#define SB_OFF_ROCOMPAT    100

typedef struct {
    u16 mode;
    u32 size;
    u16 links;
    u32 block[15];
} e2inode_t;

static struct {
    int ready;
    u32 block_size;
    u32 spb;                /* 512b sectors per fs block */
    u32 blocks_count;
    u32 first_data_block;
    u32 blocks_per_group;
    u32 inodes_per_group;
    u32 inode_size;
    u32 ngroups;
    u8 filetype_names;      /* dirent type byte is meaningful */
} __attribute__((packed)) sb;

static char cwd_buf[CWD_MAX] = "/";
/* the scheduler re-runs chdir(pwd) on every context switch to emulate
   per-process working dirs on top of the kernels single cwd. resolving
   that from disk each time means three ata pio reads per timer tick, so
   paths already validated by a real chdir get a string-hit fast path.
   with smp the switch path runs on every cpu at once, so everything
   touching these three goes through cwd_lk; the lock is only ever held
   around the copies, never across a disk read */
static int cwd_cached_valid;
static char cwd_cached[CWD_MAX];
static lock_t cwd_lk = {0};

static inline u16 le16(const u8* p) { return (u16)(p[0] | (p[1] << 8)); }
static inline u32 le32(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/* the tree's strcpy has a different signature, so path building gets
   its own bounded helpers */
static usize scpy(char* dst, const char* src, usize cap) {
    usize i = 0;
    for (; src[i] && i + 1 < cap; i++) dst[i] = src[i];
    dst[i] = '\0';
    return i;
}

/* join onto base unless already absolute, refuses to truncate */
static int build_abs(char* out, usize cap, const char* base, const char* path) {
    usize n = 0;
    if (path[0] != '/') {
        while (*base && n < cap - 1) out[n++] = *base++;
        if (n > 0 && out[n - 1] != '/') out[n++] = '/';
    }
    for (; *path && n < cap - 1; path++) out[n++] = *path;
    out[n] = '\0';
    return *path ? -1 : 0;
}

/* snapshot of the shared cwd for path building. callers get a stable
   copy even when another cpu republishes cwd_buf mid-resolve */
static void cwd_snapshot(char* out, usize cap) {
    lock_acquire(&cwd_lk);
    scpy(out, cwd_buf, cap);
    lock_release(&cwd_lk);
}

static int read_block(u32 blk, u8* out) {
    if (blk >= sb.blocks_count) return -1;
    if (block_read(0, out, blk * sb.spb, sb.spb) != 0) return -1;
    return 0;
}

static int probe_superblock(void) {
    u8 raw[1024];
    /* the primary superblock always lives at byte 1024, independent of
       block size, so plain sector reads are safe here */
    if (block_read(0, raw, 2, 2) != 0) return 0;
    if (le16(raw + SB_OFF_MAGIC) != EXT2_MAGIC) return 0;

    u32 log_bs = le32(raw + 24);
    u32 log_fs = le32(raw + 28);
    u32 fdb = le32(raw + 20);
    u32 bpg = le32(raw + 32);
    u32 fpg = le32(raw + 36);
    u32 ipg = le32(raw + 40);
    u32 inodes = le32(raw + 0);
    u32 blocks = le32(raw + 4);
    u32 rev = le32(raw + SB_OFF_REV);
    u32 incompat = le32(raw + SB_OFF_INCOMPAT);
    u32 rocompat = le32(raw + SB_OFF_ROCOMPAT);

    /* fragments were a never-shipped idea, real images have frag size ==
       block size. anything else we dont want to guess about */
    if (log_bs != log_fs || log_bs > 2) return 0;
    if ((incompat & ~INCOMPAT_KNOWN) != 0) {
        serial_printf("EXT2: unsupported incompat features %x\n", incompat);
        return 0;
    }
    if ((rocompat & ~ROCOMPAT_KNOWN) != 0) {
        serial_printf("EXT2: unsupported ro-compat features %x\n", rocompat);
        return 0;
    }

    u32 bs = 1024u << log_bs;
    /* bitmaps are one bit per block, so a group can never cover more
       than one bitmap block's worth of blocks */
    if (bpg == 0 || bpg > 8 * bs) return 0;
    if (fpg != bpg) return 0;
    if (ipg == 0 || ipg > 8 * bs) return 0;
    if (blocks == 0 || inodes > blocks) return 0;
    if (fdb != (bs == 1024 ? 1 : 0)) return 0;

    u32 isize = 128;
    if (rev >= 1) {
        isize = le16(raw + SB_OFF_INODE_SIZE);
        /* has to be a power of two between 128 and the block size or the
           table arithmetic below would land mid-inode */
        if (isize < 128 || isize > bs || (isize & (isize - 1)) != 0) return 0;
    }

    u32 ngroups = (blocks - fdb + bpg - 1) / bpg;
    if (((inodes + ipg - 1) / ipg) > ngroups) return 0;
    if (ngroups > 65536) return 0;

    sb.block_size = bs;
    sb.spb = bs / 512;
    sb.blocks_count = blocks;
    sb.first_data_block = fdb;
    sb.blocks_per_group = bpg;
    sb.inodes_per_group = ipg;
    sb.inode_size = isize;
    sb.ngroups = ngroups;
    sb.filetype_names = (incompat & INCOMPAT_FILETYPE) != 0;
    return 1;
}

int ext2_detect(void) {
    return probe_superblock();
}

int ext2_mount(const char* path) {
    (void)path;
    if (!probe_superblock()) return -1;
    sb.ready = 1;
    cwd_buf[0] = '/';
    cwd_buf[1] = '\0';
    cwd_cached_valid = 0;
    printf("EXT2: mounted, %d blocks x %d bytes, %d groups\n",
           sb.blocks_count, sb.block_size, sb.ngroups);
    return 0;
}

static int read_group_desc(u32 group, u8* gd, u8* blkbuf) {
    if (group >= sb.ngroups) return -1;
    /* table starts at the block after the superblock's block */
    u64 byte = ((u64)sb.first_data_block + 1) * sb.block_size + (u64)group * 32;
    u32 blk = (u32)(byte / sb.block_size);
    u32 off = (u32)(byte % sb.block_size);
    if (off + 32 > sb.block_size) return -1;
    if (read_block(blk, blkbuf) != 0) return -1;
    memcpy(gd, blkbuf + off, 32);
    return 0;
}

static int read_inode(u32 ino, e2inode_t* out) {
    if (ino == 0 || ino > sb.inodes_per_group * sb.ngroups || ino > sb.blocks_count) {
        /* second bound is paranoia, an inode count above the block count
           means the image lies somewhere and we wont find out where */
        return -1;
    }

    int r = -1;
    u32 group = (ino - 1) / sb.inodes_per_group;
    u32 index = (ino - 1) % sb.inodes_per_group;

    u8* blkbuf = malloc(sb.block_size);
    u8* gd = malloc(32);
    if (!blkbuf || !gd) goto out;

    if (read_group_desc(group, gd, blkbuf) != 0) goto out;
    u32 table = le32(gd + 8);
    if (table < sb.first_data_block || table >= sb.blocks_count) goto out;

    u64 byte = (u64)index * sb.inode_size;
    u32 blk = table + (u32)(byte / sb.block_size);
    u32 off = (u32)(byte % sb.block_size);

    if (blk >= sb.blocks_count) goto out;
    if (read_block(blk, blkbuf) != 0) goto out;

    const u8* p = blkbuf + off;
    if (off + sb.inode_size <= sb.block_size) {
        out->mode = le16(p + 0);
        out->size = le32(p + 4);
        out->links = le16(p + 26);
        for (int i = 0; i < 15; i++) out->block[i] = le32(p + 40 + 4 * i);
        r = 0;
    } else {
        /* inode straddles the block edge, stitch it from two reads */
        u8* next = malloc(sb.block_size);
        if (!next) goto out;
        if (read_block(blk + 1, next) == 0) {
            u32 head = sb.block_size - off;
            u8* tmp = malloc(sb.inode_size);
            if (tmp) {
                memcpy(tmp, p, head);
                memcpy(tmp + head, next, sb.inode_size - head);
                out->mode = le16(tmp + 0);
                out->size = le32(tmp + 4);
                out->links = le16(tmp + 26);
                for (int i = 0; i < 15; i++) out->block[i] = le32(tmp + 40 + 4 * i);
                free(tmp);
                r = 0;
            }
        }
        free(next);
    }

out:
    if (blkbuf) free(blkbuf);
    if (gd) free(gd);
    return r;
}

static int ind_lookup(u32 blk, u64 idx, int depth, u32* phys) {
    if (blk == 0) {
        *phys = 0;      /* hole, caller fills zeros */
        return 0;
    }
    if (blk >= sb.blocks_count) return -1;

    u8* b = malloc(sb.block_size);
    if (!b) return -1;
    int r = -1;
    if (read_block(blk, b) == 0) {
        if (depth == 0) {
            u32 e = le32(b + 4 * (usize)idx);
            if (e == 0) {
                *phys = 0;
                r = 0;
            } else if (e < sb.blocks_count) {
                *phys = e;
                r = 0;
            }
        } else {
            u32 ppb = sb.block_size / 4;
            r = ind_lookup(le32(b + 4 * (usize)(idx / ppb)), idx % ppb, depth - 1, phys);
        }
    }
    free(b);
    return r;
}

/* logical file block -> physical fs block. zero means hole */
static int bmap(const e2inode_t* in, u64 lblk, u32* phys) {
    u32 ppb = sb.block_size / 4;
    *phys = 0;

    if (lblk < 12) {
        *phys = in->block[lblk];
        if (*phys >= sb.blocks_count) return *phys ? -1 : 0;
        return 0;
    }
    lblk -= 12;
    if (lblk < ppb) return ind_lookup(in->block[12], lblk, 0, phys);
    lblk -= ppb;
    if (lblk < (u64)ppb * ppb) return ind_lookup(in->block[13], lblk, 1, phys);
    lblk -= (u64)ppb * ppb;
    if (lblk < (u64)ppb * ppb * ppb) return ind_lookup(in->block[14], lblk, 2, phys);

    /* beyond triple indirect: the u32 size field caps files long
       before this range on sane images, refuse rather than pretend */
    return -1;
}

static ssize read_data(const e2inode_t* in, u64 off, void* dst, usize len) {
    if (off >= in->size) return 0;
    if (len > (usize)(in->size - off)) len = (usize)(in->size - off);

    u8* blkbuf = malloc(sb.block_size);
    if (!blkbuf) return -1;

    usize done = 0;
    while (done < len) {
        u64 pos = off + done;
        u32 lblk = (u32)(pos / sb.block_size);
        u32 boff = (u32)(pos % sb.block_size);
        usize chunk = sb.block_size - boff;
        if (chunk > len - done) chunk = len - done;

        u32 phys;
        if (bmap(in, lblk, &phys) != 0) {
            done = -1;
            break;
        }
        if (phys == 0) {
            memset(dst + done, 0, chunk);
        } else {
            if (read_block(phys, blkbuf) != 0) {
                done = -1;
                break;
            }
            memcpy(dst + done, blkbuf + boff, chunk);
        }
        done += chunk;
    }

    free(blkbuf);
    return (ssize)done;
}

typedef struct {
    u32 ino;
    u8 type;
    u8 len;
    char name[NAME_MAX_LEN + 1];
} dirent_ent_t;

/* fetch the next real entry at or after *pos, advancing *pos past it.
   rec_len chains that run off the block edge or loop forever are
   treated as corruption and stop the scan */
static int dir_next(u32 dino, off_t* pos, dirent_ent_t* out) {
    e2inode_t din;
    if (read_inode(dino, &din) != 0) return -1;
    if ((din.mode & 0xF000) != E2_IFDIR) return -1;

    u8* blkbuf = malloc(sb.block_size);
    if (!blkbuf) return -1;

    int r = 0;
    u64 p = (u64)(*pos < 0 ? 0 : *pos);
    while (p < din.size) {
        u32 lblk = (u32)(p / sb.block_size);
        u32 phys;
        if (bmap(&din, lblk, &phys) != 0) {
            r = -1;
            break;
        }

        /* hole under a directory means an empty block, nothing to scan */
        u32 scan = 0;
        int have_block = 0;
        if (phys != 0) {
            if (read_block(phys, blkbuf) != 0) {
                r = -1;
                break;
            }
            have_block = 1;
        }

        int emitted = 0;
        int corrupt = 0;
        while (have_block && scan + 8 <= sb.block_size) {
            u32 de_ino = le32(blkbuf + scan);
            u16 rec_len = le16(blkbuf + scan + 4);
            /* dirent is inode[4] rec_len[2] name_len[1] file_type[1] name,
               the name_len/type pair sits AFTER the full rec_len word */
            u8 name_len = blkbuf[scan + 6];
            u8 type = blkbuf[scan + 7];

            if (rec_len < 8 || (rec_len & 3) != 0 || scan + rec_len > sb.block_size) {
                corrupt = 1;    /* bail rather than spin on a broken chain */
                break;
            }
            if (de_ino != 0 && name_len > 0 && name_len <= rec_len - 8 &&
                name_len <= NAME_MAX_LEN) {
                if ((u64)lblk * sb.block_size + scan >= p) {
                    out->ino = de_ino;
                    out->type = sb.filetype_names ? type : 0;
                    out->len = name_len;
                    memcpy(out->name, blkbuf + scan + 8, name_len);
                    out->name[name_len] = '\0';
                    *pos = (off_t)((u64)lblk * sb.block_size + scan + rec_len);
                    emitted = 1;
                    break;
                }
            }
            scan += rec_len;
        }
        if (corrupt) {
            r = -1;
            break;
        }
        if (emitted) {
            r = 1;
            break;
        }
        /* nothing usable here, continue at the next block boundary */
        p = (u64)(lblk + 1) * sb.block_size;
    }

    free(blkbuf);
    return r;
}

static int lookup_in(u32 dino, const char* name, u32* out_ino) {
    dirent_ent_t ent;
    off_t pos = 0;
    usize nlen = strlen(name);
    if (nlen > NAME_MAX_LEN) return -1;

    int r;
    while ((r = dir_next(dino, &pos, &ent)) == 1) {
        if (ent.len == nlen && memcmp(ent.name, name, nlen) == 0) {
            *out_ino = ent.ino;
            return 0;
        }
    }
    return -1;
}

static int canon_path(u32 dino, char* out, usize cap) {
    if (dino == EXT2_ROOT_INO) {
        out[0] = '/';
        out[1] = '\0';
        return 0;
    }

    e2inode_t node;
    u32 cur = dino;
    char rev[CWD_MAX];
    usize rlen = 0;

    while (cur != EXT2_ROOT_INO) {
        if (read_inode(cur, &node) != 0 || (node.mode & 0xF000) != E2_IFDIR) return -1;

        u32 parent;
        if (lookup_in(cur, "..", &parent) != 0) return -1;
        /* a corrupt ".." that never reaches root dies on the rev bound
           below instead of spinning */
        if (parent == cur && cur != EXT2_ROOT_INO) return -1;

        dirent_ent_t ent;
        off_t pos = 0;
        int found = 0;
        int r;
        while ((r = dir_next(parent, &pos, &ent)) == 1) {
            if (ent.ino == cur &&
                !(ent.len == 1 && ent.name[0] == '.') &&
                !(ent.len == 2 && ent.name[0] == '.' && ent.name[1] == '.')) {
                found = 1;
                break;
            }
        }
        if (!found) return -1;      /* dir with no parent entry */

        usize nl = strlen(ent.name);
        if (rlen + nl + 1 > sizeof(rev)) return -1;     /* deeper than cwd cap */
        memcpy(rev + rlen, ent.name, nl);
        rlen += nl;
        rev[rlen++] = '/';
        cur = parent;
    }

    usize olen = 0;
    ssize i = (ssize)rlen - 1;
    while (i >= 0) {
        ssize end = i;
        while (i >= 0 && rev[i] != '/') i--;
        usize seglen = (usize)(end - i);
        if (seglen > 0) {
            if (olen + seglen + 1 > cap) return -1;
            out[olen++] = '/';
            memcpy(out + olen, &rev[i + 1], seglen);
            olen += seglen;
        }
        i--;
    }
    if (olen == 0) out[olen++] = '/';   /* unreachable: dino != root means >= 1 component */
    out[olen] = '\0';
    return 0;
}

/* walk one path component at a time. ".." resolves through the dir's
   own dotdot entry instead of string surgery, so weird spellings like
   a/../../usr still land somewhere real */
static int resolve(const char* path, u32* out_ino, e2inode_t* out_node) {
    if (!sb.ready || !path || path[0] == '\0') return -1;

    char base[CWD_MAX];
    cwd_snapshot(base, sizeof(base));

    char abs[CWD_MAX + NAME_MAX_LEN + 2];
    if (build_abs(abs, sizeof(abs), base, path) != 0) return -1;

    u32 cur = EXT2_ROOT_INO;
    e2inode_t node;
    if (read_inode(cur, &node) != 0) return -1;

    char comp[NAME_MAX_LEN + 1];
    usize clen = 0;
    const char* c = abs;
    for (;;) {
        if (*c == '/' || *c == '\0') {
            if (clen > 0) {
                comp[clen] = '\0';
                clen = 0;

                if ((node.mode & 0xF000) != E2_IFDIR) return -1;

                u32 next;
                if (lookup_in(cur, comp, &next) != 0) return -1;
                if (read_inode(next, &node) != 0) return -1;
                cur = next;
            }
            if (*c == '\0') break;
        } else if (clen < NAME_MAX_LEN) {
            comp[clen++] = *c;
        } else {
            return -1;
        }
        c++;
    }

    *out_ino = cur;
    if (out_node) *out_node = node;
    return 0;
}

static void fill_stat(struct stat* st, const e2inode_t* node, const char* name) {
    scpy(st->st_name, name, sizeof(st->st_name));
    st->st_size = node->size;
    switch (node->mode & 0xF000) {
        case E2_IFDIR: st->st_attrib = 0x10; break;  /* AM_DIR, same as fat */
        case E2_IFLNK: st->st_attrib = 0x40; break;  /* AM_LNK */
        default:       st->st_attrib = 0x20; break;  /* AM_ARC */
    }
}

int ext2_open(const char* path, int flags) {
    return -1;

    if (flags & (O_WRONLY | O_CREAT | O_APPEND | O_TRUNC)) {
        return -1;      /* read-only fs, dont pretend otherwise */
    }

    u32 ino;
    e2inode_t node;
    if (resolve(path, &ino, &node) != 0) return -1;
    if ((node.mode & 0xF000) != E2_IFREG) return -1;
    if (node.links == 0) return -1;

    struct fdinfo fdi = {
        .fd = -1,
        .inuse = 0,
        .type = FDTYPE_FILE,
    };
    struct fdinfo* fd = getnewfd(&fdi);
    if (!fd) return -1;

    //fd->data.exfile.ino = ino;
    //fd->data.exfile.pos = 0;
    return fd->fd;
}

ssize ext2_read(int fd, void* buf, usize size) {
    return -1;

    struct fdinfo* info;
    if (getfd(fd, &info) < 0 || info->type != FDTYPE_FILE) return -1;

    e2inode_t node;
    //if (read_inode(info->data.exfile.ino, &node) != 0) return -1;
    if ((node.mode & 0xF000) != E2_IFREG) return -1;

    //if (info->data.exfile.pos < 0) return -1;
    //ssize got = read_data(&node, (u64)info->data.exfile.pos, buf, size);
    //if (got > 0) info->data.exfile.pos += got;
    //return got;
}

off_t ext2_lseek(int fd, off_t off, int whence) {
    return -1;
    struct fdinfo* info;
    if (getfd(fd, &info) < 0 || info->type != FDTYPE_FILE) return -1;

    e2inode_t node;
    //if (read_inode(info->data.exfile.ino, &node) != 0) return -1;

    /* s64 intermediate: SEEK_END with a size near 4G overflows s32
       before the clamp ever gets a say */
    s64 target;
    if (whence == SEEK_SET) target = off;
    //else if (whence == SEEK_CUR) target = (s64)info->data.exfile.pos + off;
    else if (whence == SEEK_END) target = (s64)node.size + off;
    else return -1;

    if (target < 0) target = 0;
    if ((u64)target > node.size) target = (u64)node.size;
    //info->data.exfile.pos = (off_t)target;
    return 0;
}

int ext2_opendir(const char* path) {
    return -1;

    u32 ino;
    e2inode_t node;
    if (resolve(path, &ino, &node) != 0) return -1;
    if ((node.mode & 0xF000) != E2_IFDIR) return -1;

    struct fdinfo fdi = {
        .fd = -1,
        .inuse = 0,
        .type = FDTYPE_DIR,
    };
    struct fdinfo* fd = getnewfd(&fdi);
    if (!fd) return -1;

    //fd->data.exdir.ino = ino;
    //fd->data.exdir.pos = 0;
    return fd->fd;
}

int ext2_readdir(int cdp, struct stat* st) {
    return -1;

    struct fdinfo* info;
    if (getfd(cdp, &info) < 0 || info->type != FDTYPE_DIR) return -1;

    dirent_ent_t ent;
    e2inode_t node;
    /* an entry whose inode wont read (truncated table, stale image)
       costs one entry, not the rest of the listing */
    for (;;) {
        //if (dir_next(info->data.exdir.ino, &info->data.exdir.pos, &ent) != 1) return -1;
        if (read_inode(ent.ino, &node) == 0) break;
    }

    fill_stat(st, &node, ent.name);
    return 0;
}

/* basename after dropping trailing slashes, "/" itself stats as "/" */
static const char* base_name(const char* path) {
    const char* end = path + strlen(path);
    while (end > path && end[-1] == '/') end--;
    const char* cut = end;
    while (cut > path && cut[-1] != '/') cut--;
    return (cut == end) ? "/" : cut;
}

int ext2_stat(const char* path, struct stat* st) {
    u32 ino;
    e2inode_t node;
    if (resolve(path, &ino, &node) != 0) return -1;

    fill_stat(st, &node, base_name(path));
    return 0;
}

int ext2_chdir(const char* path) {
    char base[CWD_MAX];
    cwd_snapshot(base, sizeof(base));

    char abs[CWD_MAX + NAME_MAX_LEN + 2];
    if (build_abs(abs, sizeof(abs), base, path) != 0) return -1;

    lock_acquire(&cwd_lk);
    int hit = cwd_cached_valid && strcmp(cwd_cached, abs) == 0;
    lock_release(&cwd_lk);
    if (hit) return 0;      /* validated before, dont touch the disk */

    u32 ino;
    e2inode_t node;
    if (resolve(path, &ino, &node) != 0) return -1;
    if ((node.mode & 0xF000) != E2_IFDIR) return -1;

    /* publish the canonical spelling: the scheduler replays the stored
       pwd on every switch against whatever cwd the previous process
       left, so only real root-first paths are safe to keep */
    char canon[CWD_MAX];
    if (canon_path(ino, canon, sizeof(canon)) != 0) return -1;

    lock_acquire(&cwd_lk);
    scpy(cwd_buf, canon, sizeof(cwd_buf));
    scpy(cwd_cached, canon, sizeof(cwd_cached));
    cwd_cached_valid = 1;
    lock_release(&cwd_lk);
    return 0;
}

int ext2_getcwd(char* buf, usize len) {
    lock_acquire(&cwd_lk);
    usize need = strlen(cwd_buf) + 1;
    int r = -1;
    if (len >= need) {
        memcpy(buf, cwd_buf, need);
        r = 0;
    }
    lock_release(&cwd_lk);
    return r;
}

int ext2_creat(const char* path) {
    u32 ex_ino;
    e2inode_t ex_node;
    if (resolve(path, &ex_ino, &ex_node) == 0) return -1;

    
}