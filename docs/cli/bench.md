# catalyst bench

```
Execute all benchmarks of a local package.


catalyst bench [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -P,     --params TEXT ...   Params to pass to the bench executable.
  -r,     --rebuild [0]       Rebuild before benchmarking.
```

## Usage
`catalyst bench [OPTIONS]`

## Options
- `-P, --params TEXT ...`: Params to pass to the bench executable.
- `-h, --help`: Print help message.

## Examples

```bash
catalyst bench
catalyst bench -- --benchmark_filter=fast.*
```

Arguments after `--` are forwarded to the benchmark executable exactly as
received from the shell. Use this form for benchmark-runner options or any
argument that Catalyst must not interpret.
