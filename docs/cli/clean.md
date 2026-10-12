# catalyst clean

```
Clean artifacts.


catalyst clean [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -p,     --profiles TEXT [[common]]  ... 
                              Profile composition to clean.
  -i,     --intermediates     Clean intermediate files only.
```

## Details
`-i/--intermediates` is supported only by the `cob` and `ninja` backends; with `gmake` it fails with an error.

## Examples

```bash
catalyst clean
catalyst clean --profiles debug
```
