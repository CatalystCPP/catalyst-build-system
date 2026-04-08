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

In addition to the console, all log messages—including `DEBUG` level messages regardless of verbose settings—are appended to a `.catalyst.log` file in the directory where Catalyst is run. 

This file is structured using JSON lines (JSONL), making it easy to parse and analyze with standard log management tools. 

### Session Events

A logging session records when it begins and ends using specific events:

```json
{"event": "begin_session", "timestamp": "2026-04-08 10:15:30"}
{"event": "end_session", "timestamp": "2026-04-08 10:16:00"}
```

### Log Entries

Standard log entries contain the timestamp, level, and message:

```json
{"level": "INFO", "message": "Building target main...", "timestamp": "2026-04-08 10:15:31"}
{"level": "DEBUG", "message": "Found dependency zlib at /usr/lib", "timestamp": "2026-04-08 10:15:32"}
{"level": "ERROR", "message": "Failed to compile source file", "timestamp": "2026-04-08 10:15:35"}
```

The structured JSON format ensures that even complex events are safely encapsulated and easy to interpret programmatically.
