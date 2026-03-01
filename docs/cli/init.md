# catalyst init

```
Initialize a new catalyst profile.

Usage: catalyst init [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -n,--name TEXT              the name of the project
  --path TEXT                 the default path for the project
  -t,--type TEXT              the project type {binary, staticlib, sharedlib, interface}
  -v,--version TEXT           the project's version
  -d,--description TEXT       a description for the project
  --provides TEXT             Artifact provided by this project.
  --cc TEXT                   the c compiler to use
  --cxx TEXT                  the cxx compiler to use
  --ccflags TEXT              c compiler flags
  --cxxflags TEXT             cxx compiler flags
  --ldflags TEXT              linker flags
  --include-dirs TEXT ...     include directories
  --source-dirs TEXT ...      source directories
  --build-dir TEXT            build directory
  --ides TEXT ...             IDEs to generate project files for {vscode, clion}
  -p,--profile TEXT           the profile to initialize
  --force-ide                 force emitting IDE config even if one already exists
```

## Examples

**Create a basic binary project:**
```bash
catalyst init --name my-tool
```

**Create a library with specific compilers:**
```bash
catalyst init --name my-lib --type staticlib --cxx g++-13
```

**Create a debug profile:**
```bash
catalyst init --profile debug --cxxflags "-g -O0"
```

**Create a project and generate IDE configurations:**
```bash
catalyst init --name my-tool --ides vscode clion
```
