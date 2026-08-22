# using AI on randomOS

LLMs are allowed here, for help, questions or actual code. there is only
one real rule: **you must understand what goes into the repo**. what it
does, what problem it solves, how it actually works. if someone asks you
about a diff you authored and you cant explain it line by line, that diff
should never have been sent.

this is a learning project. letting a model do your thinking defeats the
point of building it.

## good uses

- asking why something triple-faults or how a subsystem works
- rubber-ducking a design before you write it
- boring boilerplate you already read through and understood
- reviewing your own patch with fresh eyes

## what counts as ai slop

- pasting whole features you never read or tested
- comments narrating the obvious (`// increment i by one`)
- generic filler docs, emoji headers, "Certainly! here is..." text
- commit messages nobody would ever write by hand
- code in a style the rest of the tree doesnt use

## before you push

- boot it. `make run` and check whatever you touched actually works
- read every line as if you wrote it, because as far as the repo cares,
  you did
