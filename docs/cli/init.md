# catalyst init

```
Initialize a new catalyst profile.


catalyst init [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -n,     --name TEXT [catalyst]  
                              the name of the project
          --path TEXT [/home/som/Projects/catalyst/catalyst]  
                              the default path for the project
  -t,     --type ENUM [binary]  
                              the project type binary, staticlib, sharedlib, or interface
  -v,     --version TEXT [0.0.1]  
                              the project's version
  -d,     --description TEXT [Your Description Goes Here.]  
                              a description for the project
          --provides TEXT     Artifact provided by this project.
          --include-dirs TEXT [[include]]  ... 
                              include directories
          --source-dirs TEXT [[src]]  ... 
                              source directories
          --build-dir TEXT [build]  
                              build directory
          --ides ENUM ...     IDEs to generate project files for (vscode, clion)
  -p,     --profile TEXT [common]  
                              the profile to initialize
  -f,     --force-ide [0]     force emitting IDE config even if one already exists
```

`init` never overwrites existing files; any file that already exists is left untouched and a warning is logged.

## Examples

**Create a basic binary project:**
```bash
catalyst init --name my-tool
```

**Create a static library:**
```bash
catalyst init --name my-lib --type staticlib
```

To pick a compiler, edit `toolchain.compiler.cxx.executable` in the generated `tc_catalyst.yaml` (see [Toolchains](../concepts/toolchains.md)).

**Create a debug profile:**
```bash
catalyst init --profile debug
```

This writes `catalyst_debug.yaml`, an overlay composed on top of `catalyst.yaml`. To add debug flags, create a toolchain that sets `toolchain.extends: tc_catalyst.yaml`, sets `toolchain.compiler.cxx.flags` in it, and reference it from the debug profile's `manifest.toolchain` (see [Toolchain Inheritance](../concepts/toolchains.md#toolchain-inheritance-extends)).

**Create a project and generate IDE configurations:**
```bash
catalyst init --name my-tool --ides vscode clion
```
