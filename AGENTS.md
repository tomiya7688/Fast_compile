# Fast compile work entry

## Source of truth

- `README.md` summarizes the project goal and status.
- `Docs/Jp/README.md` is the entry point for specifications. `Docs/Jp/` is canonical; `Docs/en/` is translated material.
- Read only the Japanese chapter relevant to the task; do not load every chapter by default.

## Route by task

- Design goals and invariants: `Docs/Jp/01-設計原則.md`
- Types, ownership, and memory: `Docs/Jp/02-型とメモリ.md`
- Syntax and language semantics: `Docs/Jp/03-言語コア.md`
- Packages, interfaces, and incremental builds: `Docs/Jp/04-パッケージとビルド.md`
- Runtime, FFI, and concurrency: `Docs/Jp/05-ランタイムとFFI.md`
- Compiler, IR, optimization, and backends: `Docs/Jp/06-コンパイラと最適化.md`
- Targets and toolchains: `Docs/Jp/07-プラットフォーム.md`
- Open design decisions: `Docs/Jp/08-未決定事項.md`

## Non-negotiable project constraints

- Optimize total build time for large native projects; correctness, deterministic output, safe caching, and generated-program performance are part of the goal.
- The lightweight and optimized compiler variants share the language, parser, type system, ownership, diagnostics, interface metadata, dependency format, and core IR. Their optimizer/codegen paths differ.
- Initial target: Linux x86-64. The lightweight backend emits ELF `.o`; final linking uses an external linker, preferring mold, then `ld.lld`, then the system linker.
- The initial compiler implementation language is C.

## Working rules

- Treat the Japanese specification as binding. Keep documented decisions, open questions, and new proposals distinct. A conversational agreement not yet reflected in the specification is not documented policy.
- Search first and inspect only task-relevant files and sections. Do not read all specifications or history by default. Once the goal, required evidence, acceptance criteria, and working set are sufficient, stop exploring.
- When concurrent remote work may affect the task, check a compact remote delta through the configured GitHub connector before editing: current base commit plus relevant open PR titles, base/head, and changed file names. Read diffs or file contents only for overlapping or dependent changes; do not scan all PRs, issues, or repository history.
- When a task names a GitHub issue, fetch only that issue and its explicitly linked dependencies, then use its scope to choose the relevant specification chapter above. Do not enumerate unrelated issues.
- Do not silently narrow v0.1, invent generic compiler defaults, or add throwaway implementations that conflict with the project goals.
- Preserve unrelated working-tree changes. Run only validation relevant to the change and report unverified areas explicitly.
