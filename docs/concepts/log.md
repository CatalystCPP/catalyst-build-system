# Logging

Catalyst provides a comprehensive logging system that captures both structured data for automated analysis and colored terminal output for developers.

## Console Output

By default, Catalyst prints `INFO`, `WARN`, and `ERROR` messages to the standard output and error streams. The format for console messages is:

```text
YYYY-MM-DD HH:MM:SS [LEVEL] message
```

Each log level has a corresponding color to make scanning the console output easier:
* **DEBUG:** Purple (Requires verbose logging to be enabled)
* **INFO:** Blue
* **WARN:** Orange
* **ERROR:** Red

*Note: Errors are always routed to `std::cerr`, while other levels go to `std::cout`.*

## File Output (`.catalyst.log`)

In addition to the console, all log messages—including `DEBUG` level messages regardless of verbose settings are
written to an invocation-specific file under `.catalyst.logs/`. `.catalyst.log` is a hard link to the latest invocation's file.

Each call without `CATALYST_MACHINE` starts a new generation. At startup, Catalyst retains the
50 most recent completed generations plus all active invocations, deleting older completed logs.
A pre-existing, unbounded `.catalyst.log` is preserved as the oldest generation during migration.
Retention is by invocation, not bytes; a single invocation can still produce a large file.

Calls with `CATALYST_MACHINE` set do not rotate logs. They inherit the absolute internal
`CATALYST_LOG_PATH` and append to their parent's generation, even from a different working
directory or while another top-level build runs. A standalone machine call without an inherited
path appends to the latest local log. Each process still writes its own session markers.
OS locks protect active generations and serialize rotation; crashes release these locks automatically.

This file is structured using JSON lines (JSONL), making it easy to parse and analyze with standard log management tools.

### Session Events

!!! tip
    This can be changed by building catalyst with `-f uniform_logs`

A logging session records when it begins and ends using specific events:

```json
{"event":"begin_session","timestamp":"2025-11-07 07:59:35.537603465"}
{"event":"end_session","timestamp":"2025-11-07 09:39:19.741159607"}
```

#### Uniform Log Events

When built with uniform logs, catalyst will emit the following session markers as debug events:

```json
{"timestamp":"2025-11-07 07:59:19.735891738","level":"DEBUG","message":"begin session"}
{"timestamp":"2025-11-07 07:59:19.735891738","level":"DEBUG","message":"end session"}
```

### Log Entries

Standard log entries contain the timestamp, level, and message:

```json
{"timestamp":"2025-11-07 07:59:19.737516723","level":"INFO","message":"Test subcommand invoked."}
{"timestamp":"2025-11-07 07:59:19.735891738","level":"DEBUG","message":"catalyst test "}
{"timestamp":"2025-11-07 07:59:19.740866298","level":"ERROR","message":"Command exited with code: 32512"}
{"timestamp":"2025-12-22 13:40:53.874982115","level":"WARN","message":"Could not find library directory for vcpkg package 'reproc' at: /home/user/dev/vcpkg/packages/reproc_x64-linux/lib"}
```

The structured JSON format ensures that even complex events are safely encapsulated and easy to interpret programmatically.

### Machine Information

!!! tip
    This feature can be enabled by building catalyst with `-f log_machine_info`

When built with the `log_machine_info` feature flag, Catalyst will include the system `hostname` and process ID (`pid`) in all JSON output, including session markers and standard log entries:

```json
{"event":"begin_session","timestamp":"2026-04-08 21:10:58.527000000","hostname":"dev-workstation","pid":10425}
{"timestamp":"2026-04-08 21:10:58.527553448","level":"INFO","message":"Test subcommand invoked.","hostname":"dev-workstation","pid":10425}
```
