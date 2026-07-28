# catalyst fetch

```
Fetch all dependencies for a profile composition.


catalyst fetch [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -p,     --profiles TEXT [[common]]  ... 
                              Profile composition to fetch.
```

## Details

This command is usually run automatically by `catalyst build`, but can be run manually to prepare the environment (e.g., in CI/CD pipelines).

- **Git**: Clones repositories.
- **Vcpkg**: Generates a versioned manifest and installs packages into the
  composed profile's build tree.
- **System**: Verifies presence via pkg-config.

## Examples

```bash
catalyst fetch
catalyst fetch --profiles debug
```
