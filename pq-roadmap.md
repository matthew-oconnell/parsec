# pq-roadmap.md — Feature Requirements for `pq`

This roadmap enumerates the `jq` capabilities that `pq` must support to be a near drop‑in replacement, focusing on features actually used by the `pq-needs.sh` workflow.

## Source of Requirements

- Script analyzed: `pq-needs.sh` (complex orchestration of mesh adaptation and solver runs).
- Result: No `jq` invocations were found in the repository, including `pq-needs.sh`.
  - Workspace search for `jq`, `| jq`, `jq -r`, `jq -c` returned no matches.

Given that no `jq` features are currently exercised by `pq-needs.sh`, the hard requirements are presently "none". To ensure immediate, practical utility and drop‑in readiness when `jq` usage emerges, the following baseline and extended feature sets are recommended for `pq`.

## Baseline Compatibility (Recommended)

Essential `jq` features commonly used in shell pipelines and likely to be needed soon:

- Filter basics
  - Identity: `.`
  - Object key access: `.key`, nested paths `.a.b[0].c`
  - Array indexing and slicing: `[i]`, `[start:end]`
  - Pipe composition: `expr1 | expr2`
- Core functions
  - `length`, `keys`, `has`, `type`
  - `tostring`, `tonumber`, `select()`, `map()`, `reduce`
  - `contains`, `unique`, `sort`, `sort_by()`, `min`, `max`
  - `add`, `join`, `split`, `path`, `getpath`, `setpath`, `del`
- Construction & update
  - Object/array literals: `{...}`, `[...]`
  - Object update: `. + {k: v}`, `.|=`, `+=`, `-=`
  - Conditional: `if ... then ... else ... end`
- I/O & formatting
  - Read from stdin and files
  - Compact vs pretty output (one‑line vs indented)
  - Raw strings vs JSON (`-r` / `--raw-output`)
  - Compact output (`-c`)
  - Slurp input (`-s` / `--slurp`): read stream into array
  - Null input (`-n` / `--null-input`)
- Arguments
  - `--arg name value` (string arg)
  - `--argjson name value` (JSON arg)
  - `-f` / `--from-file` (filter file)

## Extended Compatibility (Nice to Have)

Features used in more advanced `jq` usage; implement as scope and demand justify:

- Streaming/line‑oriented processing: `--stream`
- Modules and `include`, `import` (filter libraries)
- Date/time helpers (non‑standard; optional)
- Path expressions with pattern matching: `..` recursion
- Regular expressions: `test`, `capture`, `match`, `sub`, `gsub`
- Error handling: `try`, `catch`
- Environment access: `env` (read‑only)

## CLI Option Parity

Aim for flag parity or close analogs for shell compatibility:

- `-r`, `--raw-output`: print strings without JSON quoting
- `-c`: compact output (no whitespace)
- `-n`, `--null-input`: run filters without reading input
- `-s`, `--slurp`: read all inputs into an array
- `-f`, `--from-file`: load filter from file
- `--arg name value`, `--argjson name json`
- `-M`, `--monochrome-output`: no colors (optional)
- `-S`, `--sort-keys`: sort object keys (optional)

## Behavior & Semantics

- Deterministic output: stable key ordering when requested;
  otherwise preserve input order.
- Strict, helpful errors: line/column, caret, human hints (align with `parsec`).
- UTF‑8 safe I/O; pass‑through for binary via raw mode where feasible.

## Implementation Mapping (with `parsec`)

- Parsing: leverage `parsec` to support JSON (strict) plus RON/TOML/INI/YAML as future inputs.
- Data model: `ps::Dictionary` and `Value` for unified manipulation.
- Pretty printing: use `dump(indent, compact)` to implement `-c` and pretty output modes.
- Expression engine: new component to interpret `jq`‑like filters and apply transformations over `Dictionary`.
- Error messaging: reuse `parsec` error philosophy (line/column, caret, opener locations).

## Milestones

1. CLI scaffolding (`pq` executable) with stdin/file I/O, `-r`, `-c`, `-n`, `-s`.
2. Implement core path selectors (`.key`, `[i]`, pipes), and `length`, `keys`, `has`, `type`.
3. Add `map`, `select`, `contains`, `add`, `del`, `getpath`, `setpath`.
4. Support `--arg`, `--argjson`, and filter files `-f`.
5. Extend with sorting, unique, conditionals, updates, recursion `..` as needed.
6. Performance passes and streaming (`--stream`) if required by workloads.

## Current Hard Requirements Summary

- From `pq-needs.sh`: No `jq` usage detected; immediate feature requirements are empty.
- Recommendation: Implement the Baseline Compatibility to preempt future `jq` usage and enable replacement in typical shell pipelines.
