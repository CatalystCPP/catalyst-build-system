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
| `using` | No | List of features to enable. |

```yaml
- name: nlohmann-json
  source: vcpkg
  version: 3.11.2
```

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
