# C++20 Modules

Catalyst supports C++20 **named modules** with zero configuration. This allows
you to drop a module interface unit next to your other sources, `import` it
wherever you like, and let `catalyst generate` figure the rest out.

There is nothing to declare in `CATALYST.yaml` as the import graph is discovered
automatically at generate time.

!!! warning "MVP scope"

    Module support is currently **Clang-only** and covers **named modules**.
    `import std;`, header units (`import <vector>;`), GCC, and MSVC are not
    supported yet.

## Quick Start

```
src/
  foo.cppm    # export module foo;
  main.cpp    # import foo;
```

```cpp title="src/foo.cppm"
export module foo;
export int the_answer() { return 42; }
```

```cpp title="src/main.cpp"
import foo;
#include <cstdio>
int main() { std::printf("%d\n", the_answer()); }
```

```bash
catalyst generate -b ninja
ninja -C build/common -f build.ninja
```

## Requirements

- A **Clang** toolchain, with `-std=c++20` (or later) in the toolchain's
  `compiler.cxx.flags`. Catalyst does not inject the standard flag.
- **`clang-scan-deps`** on `PATH` but only if your source set actually
  contains a module interface unit. Projects without modules never invoke the
  scanner, and their generated build files are unchanged.

## Recognized Extensions

Module interface units are discovered by extension:

| Extension | Notes |
|---|---|
| `.cppm`, `.cxxm` | Recognized natively by Clang. |
| `.ixx`, `.mpp` | Catalyst adds `-x c++-module` to the compile automatically. |
| `.cpp` (etc.) | A regular source file containing `export module M;` also works since the scan detects it and Catalyst adds `-x c++-module`. |


## How It Works

At `generate` time, when at least one interface unit is present, Catalyst runs
Clang's P1689 dependency scanner (`clang-scan-deps -format=p1689`) over every
C++ translation unit, using the exact compile flags that TU will be built with
(include dirs, defines, and `-std` all affect how imports resolve). From the
scan it learns which module each TU *provides* and which modules it *requires*,
then emits build edges where:

- an interface unit providing module `M` compiles with
  `-fmodule-output=obj/M.pcm`, producing its BMI as a side effect of the normal
  object compile;
- a TU importing `M` compiles with `-fmodule-file=M=obj/M.pcm` and gains a
  dependency edge on `M`'s **object file**, which guarantees correct build
  ordering and staleness propagation through the import graph;
- an interface unit that itself imports other modules gets both treatments.

BMIs live at `obj/<module>.pcm` inside the build directory and are located
explicitly by name — never through a search path — so the mapping is
deterministic.

Import errors are caught at generate time with the file and module named:

```
[ERROR] src/main.cpp imports module 'ghost', but no source in the set provides it
        (external/prebuilt modules are not supported yet)
```

Two sources exporting the same module name is likewise a hard error.

## Rebuild Semantics

!!! tip

    Adding or removing an `import` changes the build graph, so it requires a
    regenerate (`catalyst build -r`), exactly like adding a new source file.
    Editing a file *without* changing its imports needs no regenerate.

Staleness propagates in the import direction only: touching an interface unit
rebuilds it and everything that (transitively) imports it, in topological
order; touching a pure importer rebuilds only that file. Propagation is
timestamp-based through object files — there is no content-based cutoff, so a
comment-only edit to an interface still rebuilds its importers.

## Tooling Caveats

!!! warning "BMIs are version-locked"

    A `.pcm` file can only be read by tooling from the **same LLVM major
    version** as the compiler that produced it. If `clang-tidy` or `clangd`
    reports something like *"module file 'obj/foo.pcm' uses an older format
    that is no longer supported"*, your analysis tool and your compiler are
    different Clang versions — a common trap with editor-bundled binaries
    (VS Code's cpptools and Mason ship their own LLVM). Point your editor at
    the same `clang-tidy`/`clangd` version as the toolchain's `clang++`.

The dependency scan runs one `clang-scan-deps` process per C++ TU on every
generate. On large source sets this is noticeable; scan parallelism and caching
are planned follow-ups.
