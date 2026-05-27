# catalyst fetch

```
Fetch all dependencies for a profile composition. 


catalyst fetch [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit 
  -p,     --profiles TEXT ... 
```

## Details

This command is usually run automatically by `catalyst build`, but can be run manually to prepare the environment (e.g., in CI/CD pipelines).

- **Git**: Clones repositories.
- **Vcpkg**: Installs packages.
- **System**: Verifies presence via pkg-config.

## Examples

```bash
catalyst fetch
catalyst fetch --profiles debug
```
