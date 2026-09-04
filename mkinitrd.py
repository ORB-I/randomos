"""Pack a directory tree into a cpio "newc" archive for the kernel's initramfs.

Usage: python3 mkinitrd.py <staging_dir> <output.cpio>

The kernel (src/kern/initramfs.c) unpacks this format at boot to build the
root filesystem in memory, so no block device is required.
"""

import os
import stat
import sys


def hexfield(value, width=8):
    return format(value & 0xFFFFFFFF, "0%dx" % width)


def header(ino, mode, uid, gid, nlink, mtime, filesize,
           devmajor, devminor, rdevmajor, rdevminor, namesize):
    return ("070701"
            + hexfield(ino) + hexfield(mode) + hexfield(uid)
            + hexfield(gid) + hexfield(nlink) + hexfield(mtime)
            + hexfield(filesize) + hexfield(devmajor) + hexfield(devminor)
            + hexfield(rdevmajor) + hexfield(rdevminor)
            + hexfield(namesize) + "00000000").encode("ascii")


def align4(data):
    return data + b"\0" * ((-len(data)) % 4)


def main():
    staging, output = sys.argv[1], sys.argv[2]
    buf = bytearray()
    ino = 0

    def add(name, mode, data, dev=(0, 0)):
        nonlocal ino, buf
        ino += 1
        nlink = 2 if stat.S_ISDIR(mode) else 1
        filesize = 0 if stat.S_ISDIR(mode) or stat.S_ISLNK(mode) else len(data)
        hdr = header(ino, mode, 0, 0, nlink, 0, filesize,
                     0, 0, dev[0], dev[1], len(name) + 1)
        buf += align4(hdr + name.encode() + b"\0")
        if data:
            buf += align4(data)

    # force the root dir to inode 1 so the kernel's root_ino matches
    add(".", stat.S_IFDIR | 0o755, b"")
    ino = 1

    for root, dirs, files in os.walk(staging):
        rel = os.path.relpath(root, staging)
        if rel == ".":
            rel = ""
        for d in dirs:
            full = os.path.join(root, d)
            st = os.lstat(full)
            entry = (d if not rel else rel + "/" + d)
            add(entry, stat.S_IFDIR | (stat.S_IMODE(st.st_mode)), b"")
        for f in files:
            full = os.path.join(root, f)
            st = os.lstat(full)
            entry = (f if not rel else rel + "/" + f)
            if stat.S_ISLNK(st.st_mode):
                target = os.readlink(full)
                add(entry, stat.S_IFLNK | 0o777, target.encode())
            else:
                with open(full, "rb") as fh:
                    add(entry, stat.S_IFREG | (stat.S_IMODE(st.st_mode)),
                        fh.read())

    # trailer record terminates the archive
    trailer = header(ino + 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 11)
    buf += align4(trailer + b"TRAILER!!!\0")

    with open(output, "wb") as fh:
        fh.write(buf)

    print("mkinitrd: wrote %d bytes to %s" % (len(buf), output))


if __name__ == "__main__":
    main()