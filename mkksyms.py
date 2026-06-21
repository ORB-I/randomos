import sys
import subprocess
from dataclasses import dataclass

nm    = sys.argv[1]
file  = sys.argv[2]

@dataclass
class Symbol:
    addr: str
    name: str
    
syms: list[Symbol] = []

proc = subprocess.run([nm, file], capture_output=True, text=True)
for line in proc.stdout.splitlines():
    addr, _, name = line.split()
    syms.append(Symbol(addr, name))
    
syms.sort(key=lambda sym: int(sym.addr, 16))
    
with open('ksyms.c', 'w') as f:
    print("#include <core/debug.h>\n", file=f)
    print("__attribute__((section(\".ksyms\"))) struct kern_symbol ksymtbl[] = {", file=f)
    for sym in syms:
        print("(struct kern_symbol){ 0x" + sym.addr + ", \"" + sym.name + "\" },", file=f)
    print("};", file=f)
    print("usize nksyms = " + str(len(syms)) + ";", file=f)
    f.flush()
    f.close()