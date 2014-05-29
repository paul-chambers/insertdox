insertdox
=========

A quick hack to scan C source and insert doxygen-formatted templates before each function.

This started out pretty simple, but has grown organically over time, as my friends and I
have found other things it would be useful for it to do.

Like most code that's grown over the years to scratch a specific itch, it's not exactly
pretty. If you're not comfortable with pointer arithmetic, this is going to be unpleasant
code for you to read...

But it gets the job done; has saved many hours of tedious editing. Just don't judge how it does it :)

If I had to do it over, I'd be looking at bending LLVM to the purpose instead.
