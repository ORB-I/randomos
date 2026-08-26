# using AI on randomOS

models are welcome here -- for questions, debugging, reviews, or
writing actual code. use whatever tool you like, prompt however you
like. process isnt policed. output is.

there is one hard rule:

**you must understand what goes into the repo.**

what it does, what problem it solves, how it works, why this way and
not another. if someone asks about a diff you authored and you cant
walk through it line by line, that diff should never have been sent.
"the model said so" is not an explanation, and neither is "it boots".

this is a learning project. a model doing your thinking saves time now
and borrows understanding at interest, and the kernel already has
enough ways to fail without adding "nobody knows why this works" to
the list.


## where it earns its keep

- explaining why something triple-faults, GPFs, or deadlocks
- translating spec prose (ACPI, SDM chapters) into plain language
- rubber-ducking a design before you write it
- mechanical work you fully understand: renames across files, adding a
  syscall stub that follows an existing one, table entries
- reviewing your patch with fresh eyes before you look again
- "what am i forgetting" checks: races, leaks, missed EOI paths,
  ordering constraints between init steps


## where it falls on its face

- **hardware facts from memory.** models will confidently cite a wrong
  lapic register offset, misremember which msr gates what, or invent
  an apic bit entirely. verify against the intel sdm, the acpi spec or
  osdev -- never against vibes. for calibration: someone in this very
  repo once wrote `+ 20` where `0x20` was meant, silently read a
  reserved register instead of the id reg, and shipped it. humans do
  it. models do it too, with better grammar.

- whole features pasted in unread and untested
- refactors that touch five subsystems when you asked about one
  function. small diffs, or it didnt happen
- comments that narrate (`// increment i`). comments here say why, not
  what. if a comment only restates the code, delete it
- filler docs nobody asked for, emoji headers, "Certainly! here is..."
- commit messages no human would type, co-authored-by trailers, changelog
  noise. rewrite generated text in your own words until it sounds like
  something you'd actually say


## landmines specific to this tree

models trained on linux and glibc will assume things that are not true
here. when a suggestion cites "standard practice", ask which file it
saw that in. if it cant point into this tree, its guessing.

- freestanding, no libc. string.h/stdio.h do not exist. lib/string.c
  and lib/printf.c are ours; printf only knows what printf.c taught it
- kernel objects get `-mcmodel=kernel -mno-red-zone`. red zone use
  under interrupts corrupts state with no traceback worth reading
- inline asm is AT&T, standalone .asm files are nasm. both compile,
  mixing their syntax dies at runtime in creative places
- atomics come from stdatomic with explicit alignment -- see the note
  on ap_state.lock. sloppy alignment makes clang emit __atomic_*
  libcalls nothing can resolve
- linking goes straight through ld.lld, no -lgcc, no pie. dont propose
  flags that fight that setup
- struct layouts crossed by hand-written asm are load-bearing: if you
  reorder fields in something like intctx_t or ap_state, find every
  asm reference or add a _Static_assert so the next person finds it


## before you push

- boot it. `make run` and read the serial output for whatever you
  touched. a clean build means the compiler gave up, nothing more.
  piping works fine if you want a log: `make run 2>&1 | tee /tmp/boot.log`
- touched smp, interrupts or boot? watch wake order, eoi discipline
  and per-core state. qflags already run `-smp 2`, keep it that way
- read every line as if you wrote it. as far as the repo cares, you did
