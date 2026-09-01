import gdb
import struct

class TranslateAddr(gdb.Command):
    """Translates a virtual address using 4-level paging in GDB.
Usage: translate <pml4_vaddr> <target_vaddr>"""

    def __init__(self):
        super(TranslateAddr, self).__init__("translate", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)
        if len(args) < 2:
            print("Usage: translate <pml4_vaddr> <target_vaddr>")
            return

        pml4_vaddr = int(args[0], 16)
        target_vaddr = int(args[1], 16)

        hhdm_offset = int(gdb.parse_and_eval("hhdm_offset"))

        pml4_idx = (target_vaddr >> 39) & 0x1FF
        pdpt_idx = (target_vaddr >> 30) & 0x1FF
        pd_idx   = (target_vaddr >> 21) & 0x1FF
        pt_idx   = (target_vaddr >> 12) & 0x1FF
        offset   = target_vaddr & 0xFFF

        inferior = gdb.selected_inferior()

        def read_u64(vaddr):
            mem = inferior.read_memory(vaddr, 8)
            return struct.unpack("<Q", mem)[0]

        try:
            # Level 4: PML4
            pml4e = read_u64(pml4_vaddr + (pml4_idx * 8))
            if not (pml4e & 1):
                print(f"PML4E entry {pml4_idx} not present")
                return

            # Level 3: PDPT
            pdpte = read_u64(hhdm_offset + (pml4e & ~0xFFF) + (pdpt_idx * 8))
            if not (pdpte & 1):
                print(f"PDPTE entry {pdpt_idx} not present")
                return

            # Level 2: PD
            pde = read_u64(hhdm_offset + (pdpte & ~0xFFF) + (pd_idx * 8))
            if not (pde & 1):
                print(f"PDE entry {pd_idx} not present")
                return

            # Check 2MB Huge Page
            if pde & 0x80:
                final_phys = (pde & ~0x1FFFFF) | (target_vaddr & 0x1FFFFF)
                print(f"[Huge Page 2MB] Physical: {hex(final_phys)} -> HHDM Virt: {hex(hhdm_offset + final_phys)}")
                return

            # Level 1: PT
            pte = read_u64(hhdm_offset + (pde & ~0xFFF) + (pt_idx * 8))
            if not (pte & 1):
                print(f"PTE entry {pt_idx} not present")
                return

            final_phys = (pte & ~0xFFF) | offset
            final_virt = hhdm_offset + final_phys

            print(f"Virtual:   {hex(target_vaddr)}")
            print(f"Physical:  {hex(final_phys)}")
            print(f"HHDM Virt: {hex(final_virt)}")

        except gdb.MemoryError:
            print("Error: Inaccessible memory location encountered during page walk.")

TranslateAddr()
