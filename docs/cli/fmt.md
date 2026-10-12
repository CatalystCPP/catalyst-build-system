# catalyst fmt

```
Format project source files.


catalyst fmt [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -p,     --profiles TEXT [[common]]  ... 
                              Profile composition whose source and include directories are
                              formatted.
```

## Details

Recursively formats all C/C++ files in both the source and include directories, honouring
`.catalystignore` patterns. The formatter is taken from `manifest.tooling.FMT` (default
`clang-format`), which expects a `.clang-format` file in the project root.
