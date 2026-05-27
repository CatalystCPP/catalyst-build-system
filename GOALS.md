1. Windows Support
2. Arbitrary Toolchain Definition Support


# Windows Support

- TBD

# Toolchain Definitions

Goal: Abstract away the command-line interface semantics of the compiler so that Catalyst doesn't hardcode compiler-specific flags (like `-I`, `-D`, `-o`). Tool authors will be able to define the specific formatting of flags and commands for arbitrary toolchains via YAML definitions.

```yaml
# catalyst_toolchain_msvc.yaml
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
      command: "{cc} /c {source} /Fo\"{object}\" {ccflags} {includes} {defines}"
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

### Implementation Strategy

This is a significant architectural change that should be tackled incrementally to avoid breaking the existing build pipelines.

**Phase 1: The Internal Struct**
Create a `struct ToolchainDef` in C++ that holds all these templates as `std::string` fields. Hardcode the default GCC/Clang values into it to mimic current behavior. Pass this struct into your generators and update them to use a custom template-expansion function instead of hardcoded format strings (e.g., replace `std::format("-I{}", path)` with `expand_template(toolchain.flags.include_dir, path)`). Do not touch YAML parsing yet. If tests pass, the generators are successfully decoupled from hardcoded syntax.

**Phase 2: The YAML Parser**
Write the code to parse `catalyst_toolchain_*.yaml` files and populate the `ToolchainDef` struct.

**Phase 3: The Integration**
Wire it up so that setting `manifest.toolchain: "my_toolchain.yaml"` inside `catalyst.yaml` triggers the load of the custom toolchain definition and passes it to the generation backend.

# CMake Shims

- a way to define a catalyst project as a wrapper around CMake via shim
