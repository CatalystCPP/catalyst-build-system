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
          --ides ENUM:value in {clion->1,vscode->0} OR {1,0} ... 
                              IDEs to generate project files for
  -p,     --profile TEXT [common]  
                              the profile to initialize
  -f,     --force-ide [0]     force emitting IDE config even if one already exists
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
