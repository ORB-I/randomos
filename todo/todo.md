# randomOS - TODO

> Only open work goes here. Finished things move to [done.md](done.md).
> `// TODO:` comments in code count too, drop a reference to them here so
> they dont get forgotten.

## Open and being worked on

- [ ] More userspace utilities — @eskridd
- [ ] Work on wm — @eskridd
- [ ] errno support (libc reports positive errnos, kernel reports negative errnos) — @eskridd
- [ ] Audit most of the code — @eskridd
- [ ] VFS layer - @Wdboyes13
- [ ] devfs - @Wdboyes13

## Known issues
- [ ] Sometimes AP RUN hangs, though it reaches the "AP 1 received RUN request" print
      usually a re-link fixes it   
## Unassigned tasks
- [ ] Make a simple README.md for randomOS.
- [ ] Change About to something more professional.
- [ ] Remove dead codes.
- [ ] Add safety checks (ensurance.(c|h)) to process syscalls (src/usr/syscalls/ssc_proc)
- [ ] Add invalid flags safety checks to ensurance.(c|h) and actually use them
- [ ] Add support for different ext2 block sizes than 1024B
- [ ] Do errno stuff for src/drivers/storage/fs  

## Future tasks
- [ ] Documentation for randomOS (in near future - not for now.)