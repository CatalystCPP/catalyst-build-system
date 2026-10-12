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

If `catalyst.lock` exists in the project (or workspace root), its pinned version, hash, URL, path, triplet, port version, and vcpkg baseline override the values in the configuration.

- **Git**: Clones into `<build>/catalyst-libs/<name>`. With `latest`, performs a shallow (`--depth 1`) clone of the default branch; with a version, clones that branch shallowly; with a hash, does a full clone and checks out the hash. An existing checkout at that path is reused without fetching.
- **Vcpkg**: Runs a manifest-mode install of all vcpkg dependencies together into the composed profile's build tree, using the lockfile's builtin baseline when present.
- **Conan**: Writes `conanfile.txt` into the build directory and runs `conan install` with `--output-folder=<build>/conan`, `-g PkgConfigDeps`, and `--build=missing`. Build type and compiler settings are derived from the profile and the active toolchain.
- **Local**: Runs `catalyst build` in the dependency's directory, which fetches and builds that project's own dependencies. Cycles are detected through `CATALYST_VISITED`. Local dependencies run serially.
- **System** and **Custom**: Nothing is fetched. These are resolved at generate time (system via pkg-config, custom via the dependency's command or script).

Pre-fetch hooks run before a full fetch, and post-fetch hooks run after it. Neither runs with the local-only incremental check.

## Examples

```bash
catalyst fetch
catalyst fetch --profiles debug
```
