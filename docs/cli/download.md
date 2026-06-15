# catalyst download

```
Install a catalyst configured package from git.


catalyst download [OPTIONS] remote [branch]


POSITIONALS:
  remote TEXT REQUIRED        the remote to clone
  branch TEXT                 the branch to clone

OPTIONS:
  -h,     --help              Print this help message and exit
  -p,     --profiles TEXT [[common]]  ... 
                              the profiles to compose in the build artifact
  -f,     --features TEXT ... the features to enable in the build
  -b,     --backend TEXT      Backend to use for generation (ninja, gmake, cob).
  -t,     --target TEXT REQUIRED 
                              the path to install to
```

## Details

The `download` command automates the process of fetching a Catalyst-based project from a Git repository, building it, and installing it to a specified location. It essentially performs the following steps:

1.  **Clone**: Clones the specified git repository to a temporary directory.
2.  **Build**: Runs `catalyst build` on the cloned project with the specified profiles and features.
3.  **Install**: Installs the build artifacts to the target directory.
4.  **Cleanup**: Removes the temporary directory.

## Examples

Download and install a project to a local `bin` directory:

```bash
catalyst download https://github.com/user/repo.git --target ./bin
```

Download a specific branch with specific features enabled:

```bash
catalyst download https://github.com/user/repo.git develop --target /usr/local --features extra_feature
```

Download with custom profiles:

```bash
catalyst download https://github.com/user/repo.git --target ./dist --profiles release optimization
```
