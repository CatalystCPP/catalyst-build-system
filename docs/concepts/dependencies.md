# Dependencies

Catalyst provides built-in dependency management supporting multiple
sources. Dependencies are defined in the `dependencies` list within your profile
configuration.

## Supported Sources

### 1. `git`
Fetches a library from a remote Git repository.

| Field | Required | Description |
|---|---|---|
| `name` | Yes | Name of the dependency. |
| `source` | Yes | Must be `git`. |
| `url` | Yes | Git repository URL. |
| `version` | Yes | Tag, branch, or commit hash. |
| `using` | No | List of features to enable. |

```yaml
- name: fmt
  source: git
  url: https://github.com/fmtlib/fmt.git
  version: 10.0.0
```

### 2. `vcpkg`
Uses `vcpkg` to satisfy the dependency.

| Field | Required | Description |
|---|---|---|
| `name` | Yes | Package name in vcpkg. |
| `source` | Yes | Must be `vcpkg`. |
| `version` | No | Package version (informative only; vcpkg classic mode resolves versions based on VCPKG_ROOT's registry). |
| `triplet` | No | vcpkg triplet (e.g., `x64-linux`). |
| `linkage` | No | `static` or `shared` (default `shared`). Controls which library artifacts are scanned. |
| `transitive` | No | Automatically resolve and link the package's transitive vcpkg dependencies (default `true`). |
| `using` | No | List of features to enable. |

```yaml
- name: nlohmann-json
  source: vcpkg
  version: 3.11.2
```

#### Transitive dependencies

vcpkg packages each dependency as its own port, so a single library can rely on
several others installed alongside it (for example, `ryml` depends on `c4core`).
By default Catalyst discovers and links these automatically by reading vcpkg's
installed-package database (`$VCPKG_ROOT/installed/vcpkg/status`), so you only
need to declare the packages you use directly:

```yaml
# c4core is linked automatically as a transitive dependency of ryml.
- name: ryml
  source: vcpkg
  triplet: x64-linux
```

Build-time helper ports (such as `vcpkg-cmake`) are skipped because they ship no
link or include artifacts, and packages are emitted in a valid static-link order
(a package always precedes the packages it depends on). Set `transitive: false`
on a dependency to opt out and link only that package — useful if you prefer to
pin every transitive package explicitly.

### 3. `local`
Builds a dependency found on the local filesystem.

| Field | Required | Description |
|---|---|---|
| `name` | Yes | Name of the dependency. |
| `source` | Yes | Must be `local`. |
| `path` | Yes | Path to the dependency root. |
| `profiles`| No | Profiles to build the dependency with. |

```yaml
- name: my-lib
  source: local
  path: ../libs/my-lib
```

### 4. `system`
Uses `pkg-config` to find a system-installed library.

| Field | Required | Description |
|---|---|---|
| `name` | Yes | Name (must match `pkg-config` name). |
| `source` | Yes | Must be `system`. |
| `lib` | No | Explicit library path override. |
| `include` | No | Explicit include path override. |

```yaml
- name: openssl
  source: system
```

### 5. `conan`
Uses Conan 2.x to fetch and resolve dependencies using Conan's native `PkgConfigDeps` generator.

| Field | Required | Description |
|---|---|---|
| `name` | Yes | Name of the Conan package (e.g. `fmt`). |
| `source` | Yes | Must be `conan`. |
| `version` | Yes | Package version reference. |

```yaml
- name: fmt
  source: conan
  version: 10.1.1
```

### 6. `custom`
Runs a user-supplied command or script and parses its stdout as pkg-config-style flags
(`-I...`, `-L...`, `-l...`) — the same shape `pkg-config --cflags --libs <pkg>` would print.
Useful for emulating `FindXYZ.cmake`-style discovery logic that Catalyst has no built-in
source for.

| Field | Required | Description |
|---|---|---|
| `name` | Yes | Name of the dependency. Exported to the command/script as `CATALYST_DEP_NAME`. |
| `source` | Yes | Must be `custom`. |
| `command` | One of `command`/`script` | Inline shell command whose stdout is parsed. |
| `script` | One of `command`/`script` | Path to a script (or any executable) to run. |
| `version` | No | Exported to the command/script as `CATALYST_DEP_VERSION`. |

```yaml
- name: openssl
  source: custom
  command: "pkg-config --cflags --libs openssl"
```

```yaml
- name: mylib
  source: custom
  script: "scripts/find_mylib.sh"
  version: 1.2.3
```

The command/script must print pkg-config-style flags to stdout; a nonzero exit or output with
no recognizable `-I`/`-L`/`-l` tokens is treated as "dependency not found". It re-runs on every
`generate` (no caching), participates in neither `catalyst fetch` nor `catalyst lock` (same as
`system`), and runs with the current working directory and `PATH` of the `catalyst` process.

## Adding Dependencies via CLI


You can use the [`catalyst add`](../cli/add.md) command to append dependencies to your configuration without editing YAML manually.

```bash
catalyst add git https://github.com/fmtlib/fmt.git -v 10.0.0
catalyst add vcpkg nlohmann-json -t x64-linux
```

## Reproducible Builds with Lockfiles

Catalyst supports pinning dependency versions to ensure that everyone working on a project uses the exact same revisions.

By running [`catalyst lock`](../cli/lock.md), you generate a `catalyst.lock` file. This file pins:
- **Git** dependencies to specific 40-character commit hashes.
- **Vcpkg** dependencies to their current versions and triplets.
- **Local/System** dependencies to their paths.

When a `catalyst.lock` is present, `catalyst fetch` and `catalyst build` will prioritize its contents over the loose version constraints in `catalyst.yaml`.
