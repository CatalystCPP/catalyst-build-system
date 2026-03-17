# Workspaces

Workspaces in Catalyst allow you to manage multiple related projects (packages) within a single repository (monorepo). A workspace is defined by a `WORKSPACE.yaml` file at the root of the directory tree.

## Structure

A workspace consists of a root directory containing a `WORKSPACE.yaml` configuration file and one or more member
projects. Each member project is a standard Catalyst project with its own `catalyst.yaml`.

### Example Layout

```
my_workspace/
├── WORKSPACE.yaml
├── app/
│   ├── catalyst.yaml
│   └── src/
└── lib/
    ├── catalyst.yaml
    └── src/
```

## `WORKSPACE.yaml`

The `WORKSPACE.yaml` file is a map where keys represent workspace member identifiers,
and values define the member's configuration.

### Configuration Fields

For each member entry:

- **path** (optional): The relative path to the member's directory from the workspace root. If valid, defaults to the member identifier (key).
- **profiles** (optional): A list of profiles to apply when loading the member's configuration (e.g., `["common"]`).

### Example

```yaml
# WORKSPACE.yaml

# A library member
my_lib:
  path: libs/my_lib
  profiles: [common, release]

# An application member
my_app:
  path: apps/my_app
  profiles: [common]
```

## Discovery

Catalyst automatically detects if it is running inside a workspace by searching upwards from the current directory for
a `WORKSPACE.yaml` file.

When resolving dependencies, Catalyst can look up packages within the active workspace. This allows members to depend
on each other without needing to specify hardcoded relative paths or publish to a remote registry during development.

## Consolidated Lockfiles

In a workspace, running [`catalyst lock`](../cli/lock.md) will generate a single `catalyst.lock` at the workspace root. This consolidated lockfile contains the resolved and pinned dependencies for **all** workspace members defined in `WORKSPACE.yaml`.

This ensures that version consistency is maintained across the entire workspace, and any external dependencies used by multiple members are pinned to the same version.

## Use Cases

- Monorepos: Manage all your microservices or library sets in one place.
- Shared Development: Iterate on a library and an application consuming it simultaneously.
