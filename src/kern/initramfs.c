#include <core/std.h>
#include <core/limreqs.h>
#include <core/limine.h>
#include <core/kprint.h>
#include <core/errno.h>
#include <core/mem/vmm.h>
#include <core/liballoc.h>
#include <lib/string.h>
#include <drivers/storage/fs/vfs.h>
#include <drivers/storage/fs/ramfs.h>

/* Boot an empty root from an initramfs image instead of a block device.
 *
 * Limine loads a module (see the `module_path:` line in limine.conf) into
 * RAM and hands its physical address/size to the kernel through the
 * limine_module_request.  The image is a cpio "newc" archive (the format
 * `bsdtar --format=newc` and `gen_init_cpio` produce).  We mount a ramfs
 * backing and unpack the archive into it, so the whole root filesystem
 * lives in memory and no block device is required.
 */

/* ---- cpio "newc" archive parsing ---- */

#define CPIO_MAGIC      "070701"
#define CPIO_HDR_LEN    110
#define CPIO_TRAILER    "TRAILER!!!"
#define CPIO_ALIGN      4

static u32 cpio_hex(const char* p, usize len) {
    u32 v = 0;
    for (usize i = 0; i < len; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9')      v |= (u32)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (u32)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (u32)(c - 'A' + 10);
    }
    return v;
}

static usize cpio_align4(usize v) {
    return (v + (CPIO_ALIGN - 1)) & ~(usize)(CPIO_ALIGN - 1);
}

/* Ensure a single directory component `name` exists inside `dino`.
 * Returns its inode or a negative errno. */
static ssize initramfs_mkdir_one(vfs_t* vfs, u32 dino, const char* name) {
    ssize ino = vfs->ops->lookup(vfs, dino, name);
    if (ino >= 0) return ino;

    ino = vfs->ops->mkino(vfs, S_IFDIR | 0755, 0, 0);
    if (ino < 0) return ino;

    if (vfs->ops->mklink(vfs, (u32)ino, S_IFDIR | 0755, dino, name) < 0) {
        vfs->ops->rmino(vfs, (u32)ino);
        return -1;
    }
    return ino;
}

static int initramfs_import(vfs_t* vfs, const u8* base, u64 size) {
    u64 off = 0;

    while (off + CPIO_HDR_LEN <= size) {
        const char* hdr = (const char*)(base + off);

        if (memcmp(hdr, CPIO_MAGIC, 6) != 0) {
            kprint("initramfs: bad magic at offset %lu\n", off);
            return -EINVAL;
        }

        u32 mode     = cpio_hex(hdr + 14, 8);
        u32 filesize = cpio_hex(hdr + 54, 8);
        u32 namesize = cpio_hex(hdr + 94, 8);

        u64 nameoff = CPIO_HDR_LEN;
        u64 dataoff = cpio_align4(nameoff + namesize);
        u64 next    = cpio_align4(dataoff + filesize);

        if (off + next > size) {
            kprint("initramfs: truncated entry\n");
            return -EINVAL;
        }

        const char* name = (const char*)(base + off + nameoff);
        if (streq(name, CPIO_TRAILER)) break;

        const u8* data = base + off + dataoff;

        /* normalize: drop a leading slash and skip empty names */
        if (*name == '/') name++;
        if (*name == '\0') { off += next; continue; }

        u16 ftype = mode & 0xF000;

        usize nlen = strlen(name);
        char* path = malloc(nlen + 1);
        if (!path) return -ENOMEM;
        memcpy(path, name, nlen + 1);

        /* split into components */
        const char* comps[64];
        usize ncomps = 0;
        char* p = path;
        while (*p) {
            while (*p == '/') p++;
            if (!*p) break;
            comps[ncomps++] = p;
            if (ncomps >= 64) break;
            while (*p && *p != '/') p++;
            if (*p) *p++ = '\0';
        }

        u32 dino = vfs->root_ino;

        /* create the parent directories for all but the last component */
        bool made = false;
        for (usize i = 0; i + 1 < ncomps; i++) {
            ssize id = initramfs_mkdir_one(vfs, dino, comps[i]);
            if (id < 0) { free(path); return (int)id; }
            dino = (u32)id;
            made = true;
        }

        /* a bare directory entry: just make sure it exists */
        if (ncomps == 0 || (ftype == S_IFDIR && !made && ncomps == 1)) {
            if (ncomps == 1) {
                initramfs_mkdir_one(vfs, dino, comps[0]);
            }
            free(path);
            off += next;
            continue;
        }

        const char* leaf = comps[ncomps - 1];

        if (ftype == S_IFDIR) {
            initramfs_mkdir_one(vfs, dino, leaf);
        } else if (ftype == S_IFREG) {
            ssize ino = vfs->ops->mkino(vfs, (u16)(mode & 0xFFFF), 0, 0);
            if (ino < 0) { free(path); return (int)ino; }

            if (filesize > 0) {
                vfs->ops->write(vfs, (u32)ino, 0, filesize, (void*)data);
            }

            if (vfs->ops->mklink(vfs, (u32)ino, (u16)(mode & 0xFFFF), dino, leaf) < 0) {
                vfs->ops->rmino(vfs, (u32)ino);
            }
        } else if (ftype == S_IFLNK) {
            vfs->ops->symlink(vfs, (u16)(mode & 0xFFFF), 0, 0, (const char*)data);
        } else if (ftype == S_IFCHR || ftype == S_IFBLK) {
            u32 dmaj = cpio_hex(hdr + 78, 8);
            u32 dmin = cpio_hex(hdr + 86, 8);
            ssize ino = vfs->ops->mknod(vfs, (u16)(mode & 0xFFFF), 0, 0, MKDEV(dmaj, dmin));
            if (ino >= 0) {
                vfs->ops->mklink(vfs, (u32)ino, (u16)(mode & 0xFFFF), dino, leaf);
            }
        }

        free(path);
        off += next;
    }

    return 0;
}

int initramfs_mount(vfs_t* vfs) {
    if (!module_req.response || module_req.response->module_count == 0) {
        return -ENOENT;
    }

    /* set up a read-write ramfs backing first */
    int ret = ramfs_mount(vfs);
    if (ret < 0) return ret;

    struct limine_file* mod = module_req.response->modules[0];
    /* mod->address is a Limine higher-half virtual address (at the
     * bootloader's HHDM offset), so translate it into the kernel's own
     * HHDM mapping before reading the archive. */
    u8* addr = (u8*)xlate_limptr(mod->address);
    u64 size = mod->size;

    kprint("initramfs: unpacking %lu bytes from module\n", size);

    ret = initramfs_import(vfs, addr, size);
    if (ret < 0) {
        kprint("initramfs: failed to unpack (%d)\n", ret);
        return ret;
    }

    kprint("initramfs: mounted root at /\n");
    return 0;
}