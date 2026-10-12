# catalyst completion

```
Generate shell completion scripts.


catalyst completion [OPTIONS] shell


POSITIONALS:
  shell TEXT:{bash,zsh} REQUIRED
                              The shell to generate completion for (bash, zsh)

OPTIONS:
  -h,     --help              Print this help message and exit
```

## Examples

The script is written to stdout, so redirect it to a file.

```bash
# zsh: write to a directory on $fpath
catalyst completion zsh > "${fpath[1]}/_catalyst"

# bash
mkdir -p ~/.local/share/bash-completion/completions
catalyst completion bash > ~/.local/share/bash-completion/completions/catalyst
```
