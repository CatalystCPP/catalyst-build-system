# Toolchains

Catalyst toolchains are standalone YAML files that describe how Catalyst should
format compiler, linker, and archiver commands for a target environment. They let
projects override the built-in `gcc`-style defaults without modifying
Catalyst source code.

## Overview

Projects opt into a custom toolchain by setting `manifest.toolchain` in their
profile configuration:

```yaml
manifest:
  toolchain: msvc.yaml
```

If `manifest.toolchain` is omitted, Catalyst uses its default `gcc`
toolchain definition.

---

## Schema Overview

A Catalyst toolchain definition consists of one top-level section:

```yaml
toolchain:
  name:        # Toolchain identifier
  extensions:  # Output file extensions and prefixes
  flags:       # Include/lib/define formatting templates
  compiler:    # C and C++ compilation commands
  linker:      # Executable and shared library link commands
  archiver:    # Static library archive command
```

---

## `toolchain`

Defines the metadata and command templates Catalyst will use during generation.

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | String | `gcc` | Toolchain identifier. |
| `extends` | String | - | Optional path to a base toolchain file to inherit settings from. |
| `extensions` | Object | - | Output file extensions and library prefixes. |
| `flags` | Object | - | Templates for include paths, library paths, library names, runtime library paths, and preprocessor defines. |
| `compiler` | Object | - | C and C++ compiler executables, base flags, and command templates. |
| `linker` | Object | - | Executable and shared library linker executable, base flags, and templates. |
| `archiver` | Object | - | Static library archive template. |

!!! note

    `compiler.c.flags`, `compiler.cxx.flags`, and `linker.flags` define the
    compiler/linker flags for the target — this is the only home for build flags. The
    `manifest.tooling` flag overrides (`CCFLAGS`/`CXXFLAGS`/`LDFLAGS`) were removed in 1.7.0;
    set `compiler.c.flags`, `compiler.cxx.flags`, and `linker.flags` here instead.

### `toolchain.extensions`

| Field | Type | Default | Description |
|---|---|---|---|
| `object` | String | `.o` | Object file extension. |
| `executable` | String | `""` | Executable file extension. |
| `static_lib` | String | `.a` | Static library extension. |
| `shared_lib` | String | `.so` | Shared library extension. |
| `static_lib_prefix` | String | `lib` | Prefix used for static library filenames. |
| `shared_lib_prefix` | String | `lib` | Prefix used for shared library filenames. |
| `cpp_sources` | List of Strings | `[".cpp", ".cxx", ".cc", ".cupp"]` | List of C++ source file extensions. |
| `headers` | List of Strings | `[".h", ".hpp", ".hxx", ".hh", ".ipp", ".inl", ".tpp", ".cuh", ".tcc"]` | List of header file extensions. |
| `c_sources` | List of Strings | `[".c", ".cu"]` | List of C / CUDA source file extensions (compiled with the C compiler rule). |
| `module_interfaces` | List of Strings | `[".cppm", ".ixx", ".mpp", ".cxxm"]` | List of C++ module interface file extensions. |
| `clang_modules` | List of Strings | `[".cppm", ".cxxm"]` | List of C++ module interface extensions that do not need explicit compiler flags to specify the module compilation type. |
| `bmi` | String | `.pcm` | Compiled binary module interface (BMI) extension. |
| `library_scan` | List of Strings | Platform specific | Library extensions used to scan for vcpkg/dependency libraries (Windows defaults to `[".lib"]`; macOS defaults to `[".a", ".dylib"]`; others default to `[".a", ".so"]`). |
| `shell_scripts` | List of Strings | `[".bat", ".cmd"]` | Windows batch/shell script extensions to search when resolving paths for command executables. |

### `toolchain.flags`

| Field | Type | Default | Description |
|---|---|---|---|
| `include_dir` | String | `-I{path}` | Template for an include directory flag. |
| `lib_dir` | String | `-L{path}` | Template for a library search path flag. |
| `lib` | String | `-l{name}` | Template for a library token. |
| `rpath` | String | `-Wl,-rpath,{path}` | Template for one runtime library search path. Catalyst expands it once per unique resolved library directory. |
| `define` | String | `-D{name}={value}` | Template for a preprocessor define with a value. |
| `define_empty` | String | `-D{name}` | Template for a value-less preprocessor define. |

!!! note

    `flags` templates interpolate `{path}`, `{name}`, and `{value}` placeholders.

### `toolchain.compiler`

| Field | Type | Default | Description |
|---|---|---|---|
| `c` | Object | - | C compiler executable and command template. |
| `cxx` | Object | - | C++ compiler executable and command template. |

### `toolchain.compiler.c`

| Field | Type | Default | Description |
|---|---|---|---|
| `executable` | String | `cc` | C compiler executable. |
| `flags` | String or List | `""` | Base C compiler flags, interpolated as `{cflags}`. |
| `flags_append` | String or List | `""` | Appends flags to the inherited C compiler flags. |
| `flags_remove` | String or List | `""` | Removes matching flag tokens from the inherited C compiler flags. |
| `command` | String | `{cc} {cflags} -MMD -MF {object}.d -c {source} -o {object} {includes} {defines}` | Command template for compiling C sources. |

### `toolchain.compiler.cxx`

| Field | Type | Default | Description |
|---|---|---|---|
| `executable` | String | `c++` | C++ compiler executable. |
| `flags` | String or List | `""` | Base C++ compiler flags, interpolated as `{cxxflags}`. |
| `flags_append` | String or List | `""` | Appends flags to the inherited C++ compiler flags. |
| `flags_remove` | String or List | `""` | Removes matching flag tokens from the inherited C++ compiler flags. |
| `command` | String | `{cxx} {cxxflags} -MMD -MF {object}.d -c {source} -o {object} {includes} {defines}` | Command template for compiling C++ sources. |

!!! note

    Compiler command templates interpolate `{cc}` or `{cxx}`, plus
    `{cflags}` or `{cxxflags}`, `{source}`, `{object}`, `{includes}`, and `{defines}`.

### `toolchain.linker`

| Field | Type | Default | Description |
|---|---|---|---|
| `executable` | String | `c++` | Linker executable. |
| `flags` | String or List | `""` | Base linker flags, interpolated as `{ldflags}`. |
| `flags_append` | String or List | `""` | Appends flags to the inherited linker flags. |
| `flags_remove` | String or List | `""` | Removes matching flag tokens from the inherited linker flags. |
| `executable_command` | String | `{linker} {objects} -o {output} {ldflags} {lib_dirs} {rpaths} {libs}` | Command template for building executables. |
| `shared_lib_command` | String | `{linker} -shared {objects} -o {output} {ldflags} {lib_dirs} {rpaths} {libs}` | Command template for building shared libraries. |

!!! note

    Linker command templates interpolate `{linker}`, `{objects}`,
    `{output}`, `{ldflags}`, `{lib_dirs}`, `{rpaths}`, and `{libs}`.

    `{rpaths}` is the concatenation of `flags.rpath` expanded for every unique
    library directory reported by resolved dependencies. Set `flags.rpath` to an
    empty string to disable automatic runtime paths.

### `toolchain.archiver`

| Field | Type | Default | Description |
|---|---|---|---|
| `executable` | String | `ar` | Archive tool executable. |
| `command` | String | `{archiver} rcs {output} {objects}` | Command template for building static libraries. |

!!! note

    Archiver command templates interpolate `{archiver}`, `{output}`,
    and `{objects}`.

---

## Example

```yaml
toolchain:
  name: msvc
  extensions:
    object: .obj
    executable: .exe
    static_lib: .lib
    shared_lib: .dll
    static_lib_prefix: ""
    shared_lib_prefix: ""
  flags:
    include_dir: /I"{path}"
    lib_dir: /LIBPATH:"{path}"
    lib: "{name}.lib"
    define: /D{name}={value}
    define_empty: /D{name}
  compiler:
    c:
      executable: cl.exe
      command: '{cc} /c {source} /Fo"{object}" {cflags} {includes} {defines}'
    cxx:
      executable: cl.exe
      command: '{cxx} /EHsc /c {source} /Fo"{object}" {cxxflags} {includes} {defines}'
  linker:
    executable: link.exe
    executable_command: '{linker} {objects} /OUT:"{output}" {ldflags} {lib_dirs} {libs}'
    shared_lib_command: '{linker} /DLL {objects} /OUT:"{output}" {ldflags} {lib_dirs} {libs}'
  archiver:
    executable: lib.exe
    command: '{archiver} /OUT:"{output}" {objects}'
```

Fields omitted from a toolchain file retain their built-in defaults, so custom
toolchains only need to override the pieces that differ from the default
`gcc` behavior.

---

## Toolchain Inheritance (`extends`)

To avoid duplicating toolchain configuration across different variants
(like debug and release, or different sanitizers), a toolchain file can declare a
base it inherits from using the `extends` key under `toolchain`.

```yaml
toolchain:
  extends: tc_base.yaml
  name: "catalyst debug"
  compiler:
    cxx:
      flags: "-std=c++23 -O0 -ggdb -DDEBUG"
```

### Precedence & Merging

A toolchain configuration is resolved by folding from the base of the inheritance chain toward the leaf:

1. Start from the built-in default `gcc`-style defaults.
2. If `extends` is specified, resolve the base toolchain recursively first.
3. Apply the current file's own keys on top, overriding per leaf scalar.

Precedence order (lowest to highest):

Every unspecified key inherits from its parent base toolchain.

For flag fields (e.g. `compiler.c.flags`, `compiler.cxx.flags`, `linker.flags`), you can control composition using:

- `flags`: Replaces the inherited flag string entirely.
- `flags_append`: Appends the provided flags to the inherited flags.
- `flags_remove`: Filters out matching space-separated tokens from the inherited flags.

!!! note

    You can specify these as either a string or a list of strings. A list of strings is automatically space-separated.

!!! warning

    You should generally pick one of the two composition paradigms per block: either wholesale replacement (`flags`) OR modification (`flags_remove` and `flags_append`).

    If `flags` is specified, it completely replaces the inherited string, making `flags_remove` and `flags_append` in the exact same file largely redundant. (If multiple are used, the evaluation order is: `flags` overwrite, then `flags_remove`, then `flags_append`).

### Path Resolution

The `extends` path is resolved relative to the directory of the file that declares it, not the current
working directory. This allows a set of toolchain files to remain relative to each other even if
the build runs from a different directory. Absolute paths are used verbatim.

### Validation

- Cycle Detection: If a cycle is detected (e.g. `a.yaml` extends `b.yaml` which extends `a.yaml`, or a file extends itself), it is reported as an error.
- Missing Base: If the base file cannot be opened, it fails with an error indicating both the target file path and the referring file path.
- Depth Limit: There is a hard cap limit of 32 parent toolchain files in the inheritance chain to prevent pathological chains.
