# catalyst add

```
Add a dependency.


catalyst add [OPTIONS] [SUBCOMMANDS]


OPTIONS:
  -h,     --help              Print this help message and exit

SUBCOMMANDS:
  git                         add a remote git dependency
  system                      add a system dependency
  local                       add a local dependency
  vcpkg                       add a vcpkg dependency
  conan                       add a conan dependency
```

### git
```
add a remote git dependency


catalyst add git [OPTIONS] [remote]


POSITIONALS:
  remote TEXT                 remote git repository URL

OPTIONS:
  -h,     --help              Print this help message and exit
  -n,     --name TEXT         dependency name
  -v,     --version TEXT [latest]  
                              dependency version
  -f,     --features TEXT ... features to enable
  -p,     --profiles TEXT [[common]]  ... 
                              profiles to add the dependency to
```

### system
```
add a system dependency


catalyst add system [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -n,     --name TEXT REQUIRED 
                              dependency name
  -l,     --lib TEXT          system library name or path
  -i,     --inc TEXT          system include path
  -p,     --profiles TEXT [[common]]  ... 
                              profiles to add the dependency to
```

### local
```
add a local dependency


catalyst add local [OPTIONS] path


POSITIONALS:
  path TEXT REQUIRED          local package directory path

OPTIONS:
  -h,     --help              Print this help message and exit
  -n,     --name TEXT REQUIRED 
                              dependency name
  -p,     --profiles TEXT [[common]]  ... 
                              profiles to add the dependency to
  -f,     --features TEXT ... features to enable
```

### vcpkg
```
add a vcpkg dependency


catalyst add vcpkg [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -n,     --name TEXT REQUIRED 
                              dependency name
  -t,     --triplet TEXT REQUIRED 
                              vcpkg triplet
  -v,     --version TEXT [latest]  
                              dependency version
  -p,     --profiles TEXT [[common]]  ... 
                              profiles to add the dependency to
  -f,     --features TEXT ... features to enable
```

### conan
```
add a conan dependency


catalyst add conan [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -n,     --name TEXT REQUIRED 
                              dependency name
  -v,     --version TEXT REQUIRED 
                              dependency version
  -p,     --profiles TEXT [[common]]  ... 
                              profiles to add the dependency to
```

## Examples

```bash
# Add fmt from git
catalyst add git https://github.com/fmtlib/fmt.git -v 10.1.0

# Add fmt from vcpkg
catalyst add vcpkg fmt -t x64-linux

# Add fmt from conan
catalyst add conan -n fmt -v 10.1.1

# Add a local library to the debug profile
catalyst add local my-lib ../libs/my-lib -p debug
```
