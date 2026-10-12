# catalyst pack

```
Assemble the local package for distribution.


catalyst pack [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -p,     --profiles TEXT [[common]]  ... 
                              the profiles to compose for packing
  -s,     --source TEXT [/home/som/Projects/catalyst/catalyst]  
                              the source path to pack
  -t,     --target TEXT [build/pack]  
                              the output directory for the package
  -G,     --generators ENUM ... 
                              CPack generators to use: 7Z, APK, DEB, DMG, EXTERNAL, FREEBSD,
                              NSIS, RPM, STGZ, TGZ, TXZ, WIX, ZIP
  -a,     --all               Run all available CPack generators
  -q,     --silent            Suppress CPack output
```

## Usage
`catalyst pack [OPTIONS]`

## Options
- `-h, --help`: Print help message
- `-p, --profiles`: The profiles to compose for packing (default: `common`). This affects which artifacts and configurations get bundled.
- `-s, --source`: The source path to pack (default: current directory).
- `-t, --target`: The output directory for the generated package files (default: `build/pack`).
- `-G, --generators`: Space-separated list of CPack generators to use.
  - **Supported values:**
    - `TGZ` (Tar GZip Archive)
    - `ZIP` (Zip Archive)
    - `DEB` (Debian Package - *contact comes from `manifest.maintainer`, falling back to `manifest.author`; set one before publishing*)
    - `RPM` (Red Hat Package Manager)
    - `NSIS` (Windows Installer)
    - `WIX` (Windows Installer Toolset)
    - `DMG` (macOS DragNDrop)
    - `STGZ` (Self-extracting Tar GZip Archive)
    - `FREEBSD` (FreeBSD Package)
    - `APK` (Alpine/Android Package)
    - `7Z` (7zip Archive)
    - `TXZ` (XZ compressed Tarball)
    - `EXTERNAL` (CPack External Generator)

## Configuration
The `pack` subcommand uses several fields from the `manifest` section of your `catalyst.yaml` to populate package metadata:

- `author`: The project author.
- `maintainer`: The DEB/RPM contact (`CPACK_PACKAGE_CONTACT`). Falls back to `author`; if neither is set, a placeholder (`Unknown <unknown@example.com>`) is written, so set one before publishing.
- `vendor`: The package vendor. Defaults to `Catalyst`.
- `license_file`: Path to the license file to be included in the package.
- `readme_file`: Path to the readme file to be included in the package.

## Example
To package a release build into a TGZ archive and a DEB package:
```bash
catalyst pack -p release -G TGZ DEB
```
