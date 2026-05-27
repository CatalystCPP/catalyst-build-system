# catalyst run

```
Run a built executable. 


catalyst run [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit 
  -p,     --profile TEXT [common]  
  -P,     --params TEXT [{}]  ... 
```

## Examples

**Run the default build:**
```bash
catalyst run
```

**Run the debug build with arguments:**
```bash
catalyst run --profile debug --params "--verbose --input data.txt"
```
