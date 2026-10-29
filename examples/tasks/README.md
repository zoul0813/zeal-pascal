# Errands

A priority-ordered task list that allocates and releases linked records on the heap.

## Run

Copy this entire folder to a writable native Zeal drive. In ZShell, change into
it (`cd T:/examples/tasks` if installed there), then run `./tasks.bin`.
The included executable is standalone: it needs neither the IDE nor its ZPL library.

To edit and rebuild, start the installed IDE from this same directory (for example
`T:/bin/zpascal.bin`) and use W to open `tasks.pas`. Choose O, M, Q for
Memory output, then R. After the run, Escape returns to the IDE. For a saved
executable choose O, C, Q and C; the compiler writes `tasks.COM` and a source
map. That COM is the same native executable format as the supplied `.bin`.
The normal automatic Start and default End settings suffice.

Keep the current directory set to this folder: opening a source file does not
change the current directory. The IDE's L option selects drive roots, so using
it changes where relative data filenames resolve.

## Two-minute demo

Type `demo.in` to add three errands. L places priority 1 before priority 5;
equal priorities retain insertion order. D 2 removes the middle task. Subsequent
D 1 commands empty the list. The heap usage returns to zero; quitting also
reports `Heap bytes after cleanup: 0`.

Commands: A add (priority 1..9, text), L list, D complete a numbered task,
? help, Q quit. Lower priorities come first. The list holds at most 100 tasks;
text is truncated to 40 characters. Invalid inputs leave the list unchanged.

`demo.in` contains exact input lines, including blank lines where needed.
It is a human replay transcript and an automated-test fixture; no shell input
redirection support is assumed. End of input (Ctrl-Z) exits cleanly.

## Pascal features on show

- A forward-declared typed pointer makes a recursive record.
- `enqueue` walks pointers and links a new node in stable priority order.
- New allocates only on successful input; Dispose releases completed nodes.
- Deleting the head, middle and final node exercises different links.
- MemAvail measures heap usage; the final loop frees remaining nodes.
- Pointer tests precede dereferences in separate statements: Boolean And is eager.

## Files and native behavior

The folder contains `tasks.pas`, standalone `tasks.bin`, this README, `demo.in`.
Writes no files. The task list is deliberately held only in RAM; Q or end of
input discards it after freeing its nodes. This keeps the focus on dynamic
structures; Parts Bin demonstrates persistence. Reported bytes are relative
to the heap after the first command read (which establishes the runtime
stack allowance) and include allocator overhead. Measurements occur outside
formatting calls so their stack depth matches.

These are native Zeal programs in the port's current Turbo Pascal 3 dialect.
They use no CP/M BDOS calls or terminal-specific CRT extensions. They demonstrate
application types familiar from the CP/M era; historical binary or complete
language compatibility is not implied.
