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

The two projects pursue nearly opposite compiler-design priorities.

Bitlang emphasizes explicit, inspectable lowering stages and stable intermediate artifact boundaries. Fast compile instead prioritizes minimizing compilation work, avoiding unnecessary analysis and runtime machinery, and scaling to very large native projects with extremely short rebuild times.

In short:

- **Bitlang:** make translation stages explicit and inspectable.
- **Fast compile:** make compilation work as small, local, parallel, and cacheable as possible.

They may share an author and some engineering lessons, but Fast compile is a separate language lineage with a deliberately different design philosophy.
