# catalyst ide-sync

```
Sync IDE configuration files for an existing project.


catalyst ide-sync [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -p,     --profiles TEXT [common]  ... 
                              Profiles to use from the configuration file
          --ides ENUM ...     IDEs to generate project files for: vscode, clion
  -f,     --force-ide [0]     force emitting IDE config even if one already exists
```

## Details

The `ide-sync` command regenerates IDE project files (such as VS Code or CLion configurations) based on the current Catalyst configuration. This is useful when you've modified your project's structure, dependencies, or build settings and need to update your IDE integration.

## Examples

**Sync IDE configurations:**
```bash
catalyst ide-sync
```

**Sync for specific IDE:**
```bash
catalyst ide-sync --ides vscode
```

**Force regenerate IDE config:**
```bash
catalyst ide-sync --force-ide
```

**Sync with specific profiles:**
```bash
catalyst ide-sync --profiles common debug
```

**Sync for multiple IDEs:**
```bash
catalyst ide-sync --ides vscode clion
```
