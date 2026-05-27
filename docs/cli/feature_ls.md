# catalyst feature-ls

```
list all features across all profiles 


catalyst feature-ls [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit 
          --inverse           list no-features as well 
```

## Details

The `feature-ls` subcommand identifies all available unique features defined across all profiles in the current workspace. It searches for:

1.  Profiles defined in the top-level of `CATALYST.yaml`.
2.  Individual profile files matching the pattern `catalyst_*.yaml`.
3.  The common profile if `catalyst.yaml` exists.

It then iterates through each discovered profile, parses its `features` configuration block, and extracts all the feature names. Finally, it outputs a deduplicated, sorted list of all features.

## Examples

**List all available features:**
```bash
catalyst feature-ls
```
