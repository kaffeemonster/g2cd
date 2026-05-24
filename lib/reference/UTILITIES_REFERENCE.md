# Utilities Reference Guide

This document describes the core utility facilities provided by the G2CD library. These utilities are designed to minimize `libc` overhead, avoid unnecessary allocations in hot paths, and provide consistent behavior across platforms.

## 1. Logging Facility (`log_facility`)

The logging system is designed for high-performance execution by eliminating locking and allocation overhead in the logging hot-path employing custom lightweight vsnprintf.

### Architecture
- **TLS Buffering:** Uses Thread Local Storage (TLS) to maintain a per-thread buffer. This allows each thread to format log messages independently without synchronization.
- **Output Routing:** 
    - `LOGF_NOTICE` and above $\rightarrow$ `stdout`
    - Others $\rightarrow$ `stderr`

Ment as central abstraction for logging/output, at some point this will get wired up to `syslog()` or similar facilities.

### Primary API
| Macro/Function | Description |
| :--- | :--- |
| `logg(level, fmt, ...)` | Logs a message |
| `logg_pos(level, fmt, ...)` | Logs a message with file, function, and line number. |
| `logg_errno(level, fmt, ...)` | similar to `log_pos` and automatically appends the current `errno` string. |
| `logg_devel(str)` | Logs a develop/debug message, gets removed on release builds |
| `logg_develd(fmt, ...)` | similar to `logg_devel`, but with arguments |
| `logg_devel_old(str)` | Logs a develop/debug message, already removed, to easliy reactivate |
| `logg_develd_old(fmt, ...)` | similar to `logg_devel_old`, but with arguments |
| `die(fmt, ...)` | Logs a fatal error and terminates the process via `exit(EXIT_FAILURE)`. |
| `diedie(fmt, ...)` | Similar to `die`, but also logs `errno` |

### Log Levels
`LOGF_EMERG` $\rightarrow$ `LOGF_ALERT` $\rightarrow$ `LOGF_CRIT` $\rightarrow$ `LOGF_ERR` $\rightarrow$ `$LOGF_WARN` $\rightarrow$ `LOGF_NOTICE` $\rightarrow$ `LOGF_INFO` $\rightarrow$ `LOGF_DEBUG` $\rightarrow$  `LOGF_DEVEL` $\rightarrow$ `LOGF_DEVEL_OLD`.

`LOGF_SILENT` is a config constant to silence logging.

---

## 2. Configuration Parser (`config_parser`)

A flexible parser for `key = value` configuration files, supporting comments, quotes, and line continuations.

### Data Structure: `struct config_item`
The parser does not return a generic map; instead, it populates a predefined set of variables. This is managed via `struct config_item`:

- **`name`**: The string key to look for in the config file.
- **`data`**: A pointer to the variable where the parsed result should be stored.
- **`handler`**: A function pointer that implements the logic to parse the value and update the `data` target.

The `CONF_ITEM(str, dttt, hddd)` macro is used to define these items concisely.

### Usage Flow
To parse a configuration file, the developer defines an array of `config_item` and passes it to `config_parser_read()`:

```c
int my_port;
const char *my_name;

struct config_item my_config[] = {
    CONF_ITEM("port", &my_port, config_parser_handle_int),
    CONF_ITEM("name", &my_name, config_parser_handle_string),
};

// apply defaults to config variables
my_port = MY_DEFAULT_PORT;
my_name = "not configured";

if (!config_parser_read("settings.conf", my_config, 2)) {
    // handle error
}

// check what was read/santize
```

### Handler Pattern & Parsing Logic
The parser implements a sophisticated tokenization process:
1. **Line Processing:** Supports line continuations (using `\` at the end of a line).
2. **Tokenization:**
    - **Quotes:** Content inside `"` or `'` is treated as a single token, preserving whitespace.
    - **Comments:** Text following `#` (outside of quotes) is ignored.
    - **Whitespace:** Used to delimit keys and values.
3. **Assignment:** The parser looks for the `=` sign to separate the key from its value.
4. **Dispatch:** If a token matches a `config_item.name`, the associated `handler` is invoked.

**Key Handlers:**
- `config_parser_handle_int`: Parses signed integers into an `int *`.
- `config_parser_handle_string`: Allocates memory and parses strings into a `char **`.
- `config_parser_handle_bool`: Parses boolean values (true/false, yes/no, 1/0, t/f, y/n) into a `bool *`.
- `config_parser_handle_ip`: Parses IPv4/IPv6 addresses into a `struct combo_addr_arr *`.
- `config_parser_handle_guid`: Parses UUIDs (both colon-separated and hyphenated formats) into a `unsigned char []`, enough room has to be provided.
- `config_parser_handle_encoding`: Maps encoding strings to the `enum g2_connection_encodings`.


---

## 3. Formatting Utilities

### `vsnprintf`
A custom implementation of `vsnprintf` is used to avoid expensive `stdio` boxing and provide consistent C99-style formatting across different platforms and compiler versions and implementing custom printing extiontions for IPs, GUIDs and `errno`, improving performance in simple to normal string formatting tasks.
To call it explicitly, use `my_snprintf` and my `my_vsnprintf`. Overriding symbols in libraries can fail.
There is a downside: no positional arguments, very very fancy formating doesn't work, only basic floating point support.
An internal fast number to char converison funtion `put_dec_trunc` is also available, it outputs in reverse.

### `inet_ntop` Variants
To support different concatenation and performance needs, three variants are provided:
- `inet_ntop`: Standard API implementation.
- `inet_ntop_c`: Returns the end pointer of the written string, allowing for efficient sequential concatenation.
- `inet_ntop_c_rev`: Similar to `inet_ntop_c`, but writes the IP address in **reverse order** into the buffer.

### `print_ts` Fixed format timesttamp formating

Fixed-format UTC printing that bypasses `strftime` and `gmtime_r` overhead.

- `print_ts(buf, len, time)`: Formats a `time_t` as %Y-%m-%dT%H:%MZ into buf.
- `print_ts_rev(buf, len, time)`: similar to `print_ts`, but the output characters are reversed.
- `print_lts(buf, len, time)`: Formats a `time_t` as %Y-%m-%dT%H:%M:%SZ into buf.
- `read_ts(buf, time)`: Reads a timestamp in the format %Y-%m-%dT%H:%M from buf into time.

---

## 4. Batch printing: The Reverse Pattern & `strreverse_l`

### Performance Optimization
Formating numbers is usually done right-to-left in a tempory buffer and then a small reversing copy is done. In performance-critical paths (like batch timestamps and IP formatting), the library can avoid all these small reversals by offering functions without the reversal at the end, which then allows to reverse en-bloc when batch printing.

**Reverse-writing functions:**
- `print_ts_rev`: Writes a fixed format timestamp in reverse.
- `inet_ntop_c_rev`: like `inet_ntop`, but outputs in reverse, returns pointer to end.
- `combo_addr_print_c_rev`: like `inet_ntop_c_rev` for `combo_addr`.
- `put_dec_trunc`: put a reversed text representation of an integer into a buffer.

### The Final Flip: `strreverse_l`
Once the reverse-written data is complete, `strreverse_l` is called to reverse the entire string back to the correct order. 

**Architectural Optimizations:**
`strreverse_l` is implemented with assembly optimizations for specific architectures to ensure the reverse operation is as fast as possible:
- **x86:** Uses SSE/SSSE3/SSE2 instructions.
- **ARM:** Uses optimized word-sized swaps.
- **PPC:** Uses Altivec/Vector instructions.

---

## 5. Crash Diagnostics (`backtrace`)

Provides emergency debugging capabilities by catching critical signals.

- **Initialization:** `backtrace_init()` sets up signal handlers for `SIGSEGV`, `SIGILL`, and `SIGBUS`.
- **Output:** Upon a crash, the facility dumps CPU registers and a stack trace to `stderr`, allowing for post-mortem analysis of the failure.  Also attempts to invoke GDB/pstack.

**Warning:** This is a "hacky/unclean/portability nightmare" tool intended for emergency debugging only. Some crash debug output is better then absolutly none.

---

## Summary API Table

| Facility | Core Symbol | Thread-Safe | Purpose |
| :--- | :--- | :--- | :--- |
| **Logging** | `logg` | Yes (TLS) | High-perf diagnostic logging |
| **Config** | `config_parser_read` | No | Configuration file parsing |
| **Network** | `inet_ntop_c_rev` | Yes | Fast reverse-IP formatting |
| **Time** | `print_ts` | Yes | Fast timestamp formatting |
| **Reverse** | `strreverse_l` | Yes | Optimized string reversal |
| **Crash** | `backtrace_init` | No | Signal-based crash dumping |
