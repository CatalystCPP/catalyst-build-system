# Arbitrary Toolchain Definition Architecture Scratchpad

## Goal
Abstract away the command-line interface semantics of the compiler from Catalyst's backend generator. Catalyst should not hardcode assumptions that includes are `-I`, macros are `-D`, or outputs are `-o`.

## Strategy
Tool authors or advanced users will write a YAML file that acts as a template for mapping Catalyst's internal abstract model (sources, includes, macros, output artifacts) into exact shell strings.

## The Proposed Schema
The toolchain file (e.g., `catalyst_toolchain_msvc.yaml`) will follow this structure:

```yaml
toolchain:
  name: "msvc"

  # Extensions/Prefixes: How to name generated files correctly on this platform/toolchain
  extensions:
    object: ".obj"
    executable: ".exe"
    static_lib: ".lib"
    shared_lib: ".dll"
    static_lib_prefix: ""  # Blank for MSVC, "lib" for GCC/Clang
    shared_lib_prefix: ""

  # Flags: How to format lists of internal data
  flags:
    include_dir: "/I\"{path}\""
    lib_dir: "/LIBPATH:\"{path}\""
    lib: "{name}.lib"
    define: "/D{name}={value}"
    define_empty: "/D{name}"

  # Compilation: How to compile source code into objects
  compiler:
    c:
      executable: "cl.exe"
      command: "{cc} /c {source} /Fo\"{object}\" {ccflags} {includes} {defines}"
    cxx:
      executable: "cl.exe"
      command: "{cxx} /EHsc /c {source} /Fo\"{object}\" {cxxflags} {includes} {defines}"

  # Linking: How to link objects into binaries/DLLs
  linker:
    executable: "link.exe"
    executable_command: "{linker} {objects} /OUT:\"{output}\" {ldflags} {lib_dirs} {libs}"
    shared_lib_command: "{linker} /DLL {objects} /OUT:\"{output}\" {ldflags} {lib_dirs} {libs}"

  # Archiving: How to combine objects into static libraries
  archiver:
    executable: "lib.exe"
    command: "{archiver} /OUT:\"{output}\" {objects}"
```

## Backend Implementation Updates Needed

1. **Schema Update:** `catalyst.schema.json` must be updated to either permit the `toolchain` key as a path string OR we design a way to parse this entirely new YAML format into an internal `ToolchainDefinition` struct.
2. **Configuration Loader:** `utils/yaml/configuration.cpp` needs to parse `manifest.toolchain` path, load that YAML file, and populate the internal toolchain struct.
3. **Generator Refactor:** In `src/subcommands/generate/writers/`, the code that emits the backend build scripts (e.g., `ninja` writers) must be refactored to:
    * Look up the string template for compiling C/C++.
    * Substitute the `{cc}`, `{cxx}`, `{source}`, `{object}`, `{includes}`, etc., placeholders using the actual configured values for the target package.
    * Do the same for formatting individual includes/defines using the `flags` templates before substituting them into the main command.
4. **Fallback:** If no `toolchain` file is provided, Catalyst should instantiate a default internal `ToolchainDefinition` that represents the current `gcc`/`clang` compliant behavior (using `-I`, `-D`, `-o`, etc.).
