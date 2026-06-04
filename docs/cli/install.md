# catalyst install

```
Install the build artifacts


catalyst install [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -p,     --profiles TEXT [[common]]  ...
                              the profiles to compose in the build artifact
  -s,     --source TEXT [PWD]
                              the source of the path to build
  -t,     --target TEXT REQUIRED
                              the path to install to
```

## Description

The `install` command copies the built artifacts (executables, libraries) and public headers to a specified installation directory. It organizes them into standard subdirectories (`bin/`, `lib/`, `include/`) within the target path.

You must run `catalyst build` before running `catalyst install`.

## Examples

**Install to a local `dist` folder:**
```bash
catalyst install --target ./dist
```

**Install a debug build:**
```bash
catalyst install --target ./dist --profiles debug
```

**Install from a specific source directory:**
```bash
catalyst install --source ./my-project --target /opt/my-project
```
