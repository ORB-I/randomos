# randomOS - TODO

# THIS IS NO LONGER USED
# THIS HAS BEEN MOVED TO https://github.com/users/Wdboyes13/projects/3  

> Only open work goes here. Finished things move to [done.md](done.md).
> `// TODO:` comments in code count too, drop a reference to them here so
> they dont get forgotten.

## In progress

- [ ] More userspace utilities — @eskridd
- [ ] Work on wm — @eskridd
- [ ] errno support (libc reports positive errnos, kernel reports negative errnos) — @eskridd
- [ ] Audit most of the code — @eskridd

## Know bugs
- [ ] Sometimes AP RUN hangs, though it reaches the "AP 1 received RUN request" print
      usually a re-link fixes it  
## Unassigned tasks
- [ ] Add safety checks (ensurance.(c|h)) to process syscalls (src/usr/syscalls/ssc_proc)
- [ ] Add invalid flags safety checks to ensurance.(c|h) and actually use them
- [ ] Add support for different ext2 block sizes than 1024B  
- [ ] Do errno stuff for src/drivers/storage/fs  
- [ ] Change scheduler to use multiple cores  
- [ ] devtmpfs  
- [ ] tmpfs  
- [ ] vfs  
- [ ] Add permissions checks so that permissions actually have a point  
- [ ] Make it so that setuid/seteuid/setgid/setegid check if setuid/setgid bit is set on executable  
- [ ] Add host functions specific to LwIPs sockets API  
- [ ] More ethernet drivers  
- [ ] VirtIO support  
- [ ] xHCI support alongside UHCI  
- [ ] More RNG drivers  

## Future tasks
- [ ] Documentation for randomOS (in near future - not for now.)