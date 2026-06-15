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
  remote TEXT                 

OPTIONS:
  -h,     --help              Print this help message and exit
  -n,     --name TEXT         
  -v,     --version TEXT [latest]  
  -f,     --features TEXT ... 
  -p,     --profiles TEXT ... 
```

### system
```
add a system dependency


catalyst add system [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -n,     --name TEXT REQUIRED 
  -l,     --lib TEXT          
  -i,     --inc TEXT          
  -p,     --profiles TEXT ... 
```

### local
```
add a local dependency


catalyst add local [OPTIONS] path


POSITIONALS:
  path TEXT REQUIRED          

OPTIONS:
  -h,     --help              Print this help message and exit
  -n,     --name TEXT REQUIRED 
  -p,     --profiles TEXT ... 
  -f,     --features TEXT ... 
```

### vcpkg
```
add a vcpkg dependency


catalyst add vcpkg [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -n,     --name TEXT REQUIRED 
  -t,     --triplet TEXT REQUIRED 
  -v,     --version TEXT [latest]  
  -p,     --profiles TEXT [[common]]  ... 
  -f,     --features TEXT ... 
```

### conan
```
add a conan dependency


catalyst add conan [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -n,     --name TEXT REQUIRED 
  -v,     --version TEXT REQUIRED 
  -p,     --profiles TEXT [[common]]  ... 
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
