# Tools for Nontrivial Parameters

This is something of a case study in how easy it is to write new tools to
detect a particular feature of code, beyond what the compiler already knows
how to do.  In particular, we're looking for "nontrivial" parameters which
may require extra code to pass by value in C or C++.

The only features this looks for so far are:

- The parameter is a struct or class type.
- The declaration is not in a system file.  If this can't be exactly
  determined, approximate "system" as paths starting with `/usr/`, but not
  `/usr/src/debug/` as found in distro debuginfo packages.

Whether this is a particularly interesting feature of parameters is somewhat
beside the point.  It's interesting in its own just how easy or hard it is to
write such an analysis tool with different libraries.

Additionally, each tool tries to mimic the same compiler-like output, so they
may easily be parsed by things like Vim quickfix.


## Using gcc-python-plugin

This is the approach taken in the [original blog post] using
[gcc-python-plugin], and it is clearly the simplest.  Find the source in
[note-nontrivial-param.py](./note-nontrivial-param.py).  Here it is in action:

```
$ gcc-with-python3 note-nontrivial-param.py -g -c test.cc
test.cc: In function ‘size_t len(std::string)’:
test.cc:2:30: note: parameter type is not trivial
 size_t len(const std::string s) { return s.size(); }
                              ^
```

### Advantages

- It gets to leverage the compiler for the nice location information,
  including the column number and the associated source line with a caret
  marker.
- It could be changed to a *warning* rather than a *note*, and then stop the
  build when `-Werror` is used.

### Disadvantages

- It can't be used with other compilers. (However, others may have their own
  plugin technique.)
- It can't analyze any existing binaries, only source.


## Using elfutils

This tool uses `libdw` and `libdwfl` from [elfutils], and the code is in
[elfutils-nontrivial-param.c](./elfutils-nontrivial-param.c).

### Advantages

### Disadvantages


## Using Dyninst

This tool uses [Dyninst SymtabAPI], and the code is in
[dyninst-nontrivial-param.c](./dyninst-nontrivial-param.c).

### Advantages

### Disadvantages


## Using libdwarf

This tool uses [libdwarf] (formerly from SGI), and the code is in
[libdwarf-nontrivial-param.c](./libdwarf-nontrivial-param.c).

### Advantages

### Disadvantages


[original blog post]: http://blog.cuviper.com/2014/01/23/add-new-warnings-to-gcc-with-python/
[gcc-python-plugin]: https://fedorahosted.org/gcc-python-plugin/
[elfutils]: https://fedorahosted.org/elfutils/
[Dyninst SymtabAPI]: http://www.dyninst.org/symtab
[libdwarf]: http://www.prevanders.net/dwarf.html
