# Toolchains

Catalyst toolchains are standalone YAML files that describe how Catalyst should
format compiler, linker, and archiver commands for a target environment. They let
projects override the built-in `gcc`-style defaults without modifying
Catalyst source code.

## Overview

Projects opt into a custom toolchain by setting `manifest.toolchain` in their
configuration:

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
| `extensions` | Object | - | Output file extensions and library prefixes. |
| `flags` | Object | - | Templates for include paths, library paths, library names, and preprocessor defines. |
| `compiler` | Object | - | C and C++ compiler executables, base flags, and command templates. |
| `linker` | Object | - | Executable and shared library linker executable, base flags, and templates. |
| `archiver` | Object | - | Static library archive template. |

> **Note**: `compiler.c.flags`, `compiler.cxx.flags`, and `linker.flags` define the
> compiler/linker flags for the target — this is the only home for build flags. The
> `manifest.tooling` flag overrides (`CCFLAGS`/`CXXFLAGS`/`LDFLAGS`) were removed in 1.7.0;
> set `compiler.c.flags`, `compiler.cxx.flags`, and `linker.flags` here instead.

### `toolchain.extensions`

| Field | Type | Default | Description |
|---|---|---|---|
| `object` | String | `.o` | Object file extension. |
| `executable` | String | `""` | Executable file extension. |
| `static_lib` | String | `.a` | Static library extension. |
| `shared_lib` | String | `.so` | Shared library extension. |
| `static_lib_prefix` | String | `lib` | Prefix used for static library filenames. |
| `shared_lib_prefix` | String | `lib` | Prefix used for shared library filenames. |

### `toolchain.flags`

| Field | Type | Default | Description |
|---|---|---|---|
| `include_dir` | String | `-I{path}` | Template for an include directory flag. |
| `lib_dir` | String | `-L{path}` | Template for a library search path flag. |
| `lib` | String | `-l{name}` | Template for a library token. |
| `define` | String | `-D{name}={value}` | Template for a preprocessor define with a value. |
| `define_empty` | String | `-D{name}` | Template for a value-less preprocessor define. |

> **Note**: `flags` templates interpolate `{path}`, `{name}`, and `{value}` placeholders.

### `toolchain.compiler`

| Field | Type | Default | Description |
|---|---|---|---|
| `c` | Object | - | C compiler executable and command template. |
| `cxx` | Object | - | C++ compiler executable and command template. |

### `toolchain.compiler.c`

| Field | Type | Default | Description |
|---|---|---|---|
| `executable` | String | `cc` | C compiler executable. |
| `flags` | String | `""` | Base C compiler flags, interpolated as `{cflags}`. |
| `command` | String | `{cc} {cflags} -MMD -MF {object}.d -c {source} -o {object} {includes} {defines}` | Command template for compiling C sources. |

### `toolchain.compiler.cxx`

| Field | Type | Default | Description |
|---|---|---|---|
| `executable` | String | `c++` | C++ compiler executable. |
| `flags` | String | `""` | Base C++ compiler flags, interpolated as `{cxxflags}`. |
| `command` | String | `{cxx} {cxxflags} -MMD -MF {object}.d -c {source} -o {object} {includes} {defines}` | Command template for compiling C++ sources. |

> **Note**: Compiler command templates interpolate `{cc}` or `{cxx}`, plus
> `{cflags}` or `{cxxflags}`, `{source}`, `{object}`, `{includes}`, and `{defines}`.

### `toolchain.linker`

| Field | Type | Default | Description |
|---|---|---|---|
| `executable` | String | `c++` | Linker executable. |
| `flags` | String | `""` | Base linker flags, interpolated as `{ldflags}`. |
| `executable_command` | String | `{linker} {objects} -o {output} {ldflags} {lib_dirs} {libs}` | Command template for building executables. |
| `shared_lib_command` | String | `{linker} -shared {objects} -o {output} {ldflags} {lib_dirs} {libs}` | Command template for building shared libraries. |

> **Note**: Linker command templates interpolate `{linker}`, `{objects}`,
> `{output}`, `{ldflags}`, `{lib_dirs}`, and `{libs}`.

### `toolchain.archiver`

| Field | Type | Default | Description |
|---|---|---|---|
| `executable` | String | `ar` | Archive tool executable. |
| `command` | String | `{archiver} rcs {output} {objects}` | Command template for building static libraries. |

> **Note**: Archiver command templates interpolate `{archiver}`, `{output}`,
> and `{objects}`.

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
