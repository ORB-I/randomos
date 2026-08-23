# randomOS — TODO

> Only open work goes here. Finished things move to [done.md](done.md).
> `// TODO:` comments in code count too, drop a reference to them here so
> they dont get forgotten.

## In progress

## Open

- [ ] Audit most of the code — @eskridd

## Known issues


## Design debt

- [ ] dead processes are never reaped, pid slots are gone forever and
      NEWPROC starts failing after roughly 254 spawns
- [ ] SYS_EXIT throws its exit code away, WAIT can only ever report pids

## No owner yet

- [ ] Documentation for randomOS ( in near future )
