# Fast compile

**Fast compile** is an experimental programming language designed around one primary goal:

> Compile fast.

The project intentionally prefers simple, predictable language rules over expensive compiler analysis.

## Current direction

The first confirmed design work is the memory model:

- No garbage collector.
- Heap-owned values are automatically released when their owner leaves scope.
- A heap-owned value has one owner.
- Ownership transfer is explicit with `move`.
- Borrowed references use `&T`.
- Borrowed references do not escape the function boundary that owns their validity.
- Returning or storing an escaping borrowed reference is not supported.
- Owned values can cross function boundaries by ownership transfer.
- The compiler should reject invalid operations locally instead of performing expensive cross-function lifetime analysis.

Example syntax is currently provisional.

```text
fn create() -> Data {
    let x = new Data()
    return move x
}

fn use(data: &Data) {
    print(data)
}
```

See [docs/DESIGN.md](docs/DESIGN.md) for the current design decisions.

## Status

Very early design stage. Syntax, type system, compiler backend, standard library, and file extension are not decided yet.


## Relationship to Bitlang

Fast compile is developed by the same author as Bitlang, but it is intentionally **not part of the Bitlang language family**.

The two projects pursue almost opposite ideas of what a compiler should become.

Bitlang deliberately expands compilation into a large and highly capable transformation system. Its preprocessor and compiler are intended to go far beyond ordinary parsing and static analysis, moving toward sophisticated source-to-source transformation and text-processing behavior that approaches a lightweight text-AI-like role.

That design gives Bitlang broad compatibility, translation flexibility, and the ability to reshape source through rich intermediate processing.

Fast compile intentionally gives up most of that flexibility.

It avoids broad compatibility layers, open-ended source transformation, expensive analysis, large compile-time execution systems, and other machinery that would make compilation harder to predict or scale.

In short:

- **Bitlang:** make compilation itself extremely powerful, flexible, and transformative.
- **Fast compile:** remove as much compilation work as possible while preserving a useful, fast native language.

Fast compile is therefore not a reduced Bitlang implementation. It is a separate language lineage created by choosing the opposite tradeoff.

Both approaches have value: Bitlang explores how much capability can be placed into compilation, while Fast compile explores how much capability can be removed while still producing a practical large-scale systems language.

