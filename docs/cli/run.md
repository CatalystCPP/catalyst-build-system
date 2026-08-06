# catalyst run

```
Run a built executable.


catalyst run [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -p,     --profiles TEXT [[common]]  ... 
                              Profile composition to run.
  -P,     --params TEXT ...   
```

## Examples

**Run the default build:**
```bash
catalyst run
```

**Run the debug build with arguments:**
```bash
catalyst run --profiles debug -- --verbose --input data.txt
```

Arguments after `--` are forwarded to the built executable exactly as received
from the shell. Use this form for options or any argument that Catalyst must not
interpret.
