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
appended to a `.catalyst.log` file in the directory where Catalyst is run.

This file is structured using JSON lines (JSONL), making it easy to parse and analyze with standard log management tools.

### Session Events

> [!TIP] This can be changed by building catalyst with -f uniform_logs

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
