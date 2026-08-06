# catalyst test

```
Run the test executable.


catalyst test [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -P,     --params TEXT ...   Params to pass to the test executable.
  -r,     --rebuild [0]       Rebuild before testing.
```

## Examples

```bash
catalyst test
catalyst test --params "--gtest_filter=MyTest.*"
catalyst test -- '[toolchain]'
catalyst test -- --gtest_filter=MyTest.*
```

Arguments after `--` are forwarded to the test executable exactly as received
from the shell. Use this form for Catch2 tag expressions, test-runner options,
or any argument that Catalyst must not interpret.
