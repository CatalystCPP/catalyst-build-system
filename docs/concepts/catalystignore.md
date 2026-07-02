# Catalyst Ignore

The `.catalystignore` file allows you to exclude specific source files from the build based on the active [profiles](profiles.md). This is useful for excluding platform-specific code, tests, or benchmarks from certain build types.

## Location

A `.catalystignore` file should be placed at the root of any directory listed in your `manifest.dirs.source` (as defined in `catalyst.yaml`).

## Format

The file uses YAML syntax. Top-level keys correspond to profile names, and their values are lists of regular expression patterns to ignore.

```yaml
# src/.catalystignore
debug:
  - "test_.*.cpp"
  - ".*_benchmark.cpp"

release:
  - ".*_debug.cpp"

linux:
  - ".*_win32.cpp"
```

## How it Works

1.  **Profile-Based Filtering**: Catalyst only applies the ignore patterns for the profiles that are explicitly passed to the `build` or `generate` commands via the `--profiles` (or `-p`) flag.
2.  **Regex Matching**: Patterns are treated as regular expressions. They are matched against the **filename** of each source file, not the full path. For example, the pattern `test_.*\.cpp` will match `src/test_main.cpp` but it won't match `src/tests/main.cpp` if you were trying to match the directory name (because it only matches against the filename `main.cpp`).
3.  **Recursive Search**: Catalyst recursively searches your source directories for files. The patterns defined in the `.catalystignore` at the root of a source directory apply to all files within that directory and its subdirectories.
4.  **Automatic Inclusion**: By default, Catalyst only considers files with the following extensions as source files:
    *   `.cpp`, `.cxx`, `.cc`
    *   `.c`
    *   `.cu`, `.cupp` (CUDA)

    `.catalystignore` filters this set of files further.

## Example

Suppose you have the following project structure:

```
my-project/
├── catalyst.yaml
└── src/
    ├── .catalystignore
    ├── main.cpp
    ├── util.cpp
    ├── util_linux.cpp
    └── util_win32.cpp
```

And `src/.catalystignore` contains:

```yaml
linux:
  - ".*_win32.cpp"
windows:
  - ".*_linux.cpp"
```

If you build with `--profiles linux`, the file `util_win32.cpp` will be excluded from the build.
