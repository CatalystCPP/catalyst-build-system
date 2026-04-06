# Toolchains

Catalyst relies on **toolchains** to decouple the core generation engine from hardcoded compiler and linker syntaxes. By abstracting compiler commands, flag formatting, and file extensions into configurable templates, Catalyst supports arbitrary languages and toolchains without requiring C++ source code modifications.

## Concept

When you run `catalyst build` or `catalyst run`, Catalyst parses the `manifest.toolchain` field from your project's configuration to identify how it should compile, link, and manage dependencies for the specific target environment. If no toolchain is specified, Catalyst uses a default set of templates compatible with `gcc` and `clang`.

## Toolchain Definition File

A toolchain is defined via a standalone YAML file containing key metadata, file extension mappings, flag formatting templates, and compilation commands. Variables can be interpolated into strings using `{placeholder}` formatting. 

Here is the schema for a custom `toolchain.yaml` definition:

```yaml
# my_toolchain.yaml
toolchain:
  name: "msvc"

  # 1. File Extensions and Prefixes
  extensions:
    object: ".obj"
    executable: ".exe"
    static_lib: ".lib"
    shared_lib: ".dll"
    static_lib_prefix: ""
    shared_lib_prefix: ""

  # 2. Flag Templates
  flags:
    include_dir: "/I\"{path}\""
    lib_dir: "/LIBPATH:\"{path}\""
    lib: "{name}.lib"
    define: "/D{name}={value}"
    define_empty: "/D{name}"

  # 3. Compilation Commands
  compiler:
    c:
      executable: "cl.exe"
      command: "{cc} /c {source} /Fo\"{object}\" {cflags} {includes} {defines}"
    cxx:
      executable: "cl.exe"
      command: "{cxx} /EHsc /c {source} /Fo\"{object}\" {cxxflags} {includes} {defines}"

  # 4. Linking Commands
  linker:
    executable: "link.exe"
    executable_command: "{linker} {objects} /OUT:\"{output}\" {ldflags} {lib_dirs} {libs}"
    shared_lib_command: "{linker} /DLL {objects} /OUT:\"{output}\" {ldflags} {lib_dirs} {libs}"

  # 5. Archiving Commands
  archiver:
    executable: "lib.exe"
    command: "{archiver} /OUT:\"{output}\" {objects}"
```

## Using a Toolchain

To apply your custom toolchain to a Catalyst project, configure your `catalyst.yaml` to point to the file via the `manifest.toolchain` key. 

```yaml
common:
  manifest:
    name: my_project
    type: BINARY
    toolchain: my_toolchain.yaml
```

During dependency resolution and rule generation, Catalyst automatically loads the specified toolchain file, parses the templates, and maps native flags and command lines dynamically onto the generated build configurations (e.g. `build.ninja`, `Makefile`, etc.).
