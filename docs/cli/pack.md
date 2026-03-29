# catalyst pack

Assemble the local package for distribution. This command delegates to `cpack` internally, setting up the necessary configuration to generate redistributable archives or installers for your compiled project.

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
    - `DEB` (Debian Package - *requires a valid contact/author field in `catalyst.yaml`*)
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
- `maintainer`: The project maintainer/contact (essential for `DEB` and `RPM`).
- `vendor`: The project vendor.
- `license_file`: Path to the license file to be included in the package.
- `readme_file`: Path to the readme file to be included in the package.

## Example
To package a release build into a TGZ archive and a DEB package:
```bash
catalyst pack -p release -G TGZ DEB
```
