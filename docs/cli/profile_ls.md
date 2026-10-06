# catalyst profile-ls

```
list all profiles


catalyst profile-ls [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
```

## Details

The `profile-ls` subcommand identifies all available profiles in the current directory. Output is sorted and deduplicated. With `--json`, stdout contains an array of profile-name strings (or `[]` if none are found). An unreadable, malformed, or non-mapping `CATALYST.yaml` causes a nonzero exit instead of a partial listing. It searches for:

1.  Profiles defined in the top-level of `CATALYST.yaml`.
2.  Individual profile files matching the pattern `catalyst_*.yaml`.
3.  The common profile if `catalyst.yaml` exists.

## Examples

**List all available profiles:**
```bash
catalyst profile-ls
```

**Use profiles in scripts:**
```bash
catalyst profile-ls --json | jq -r '.[]'
```
