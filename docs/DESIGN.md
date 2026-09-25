# Fast compile — Design Decisions

This document records only decisions that have been agreed on so far.

Syntax shown in examples is provisional unless explicitly specified otherwise.

## 1. Primary goal

Fast compile prioritizes **compiler speed and predictability**.

When there is a choice between:

- a sophisticated feature that requires expensive whole-program analysis, and
- a simpler rule that can be checked locally,

the language should generally prefer the simpler local rule.

The compiler should not inspect unrelated function bodies merely to decide whether ordinary code is valid.

## 2. Garbage collection

Fast compile does **not** use a garbage collector.

Memory reclamation should not depend on a tracing GC running during program execution.

## 3. Ownership

Heap-owned values have a single owner.

```text
let data = new Data()
```

Here, `data` owns the allocated value.

When the owner leaves its scope, the value is automatically destroyed/freed.

```text
fn example() {
    let data = new Data()
    use(&data)
} // data is automatically released here
```

This also applies to early returns and other normal exits from the scope.

## 4. Explicit ownership transfer

Ownership transfer is explicit.

```text
let a = new Data()
let b = move a

use(b) // OK
use(a) // invalid: a no longer owns a value
```

After a move, the source variable is no longer usable as an owned value.

An owned value may cross a function boundary by moving ownership.

```text
fn create() -> Data {
    let value = new Data()
    return move value
}
```

## 5. Borrowed references

A borrowed reference is written as `&T`.

```text
fn show(value: &Data) {
    print(value)
}
```

A borrow does not transfer ownership.

```text
let data = new Data()
show(&data)
use(data) // still owned here
```

## 6. Borrowed references do not escape

Fast compile does not attempt a general-purpose lifetime proof across functions.

A borrowed reference is valid only as a temporary view within the permitted local/function-call context. It cannot escape into a longer-lived location.

For example, returning a borrowed reference is not supported:

```text
fn bad() -> &Data {
    let data = new Data()
    return &data // invalid
}
```

Likewise, code cannot make a borrowed reference outlive the context in which it is valid by storing it in a global, object, container, or another escaping location.

The important principle is:

> The compiler should not follow a pointer through other functions to determine whether it is safe.

Instead, the operation itself should simply be invalid under the local type/usage rules.

## 7. Local errors instead of lifetime investigation

Invalid reference operations should fail where they are written.

Conceptually, the compiler should behave more like:

```text
expected: owned Data
found:    borrowed &Data
```

than performing a complex analysis and reporting a long lifetime proof.

In other words, for unsupported pointer/reference behavior, the language can effectively say:

> There is no such operation here.

This is intentional. Restrictions in the language are used to keep compilation simple and fast.

## 8. Automatic destruction and manual free

The normal path is automatic destruction when ownership ends.

A manual `free`-style operation may also be supported for an owned value:

```text
let data = new Data()
free(data)
```

After explicit release, the value is no longer usable.

```text
free(data)
free(data) // invalid
```

This can be checked using simple local state such as:

- alive
- moved
- freed
- borrowed

The language should avoid requiring general interprocedural lifetime inference for this bookkeeping.

## 9. Function boundaries

Current intended behavior:

| Operation | Status |
| --- | --- |
| Pass an owned value by move | Allowed |
| Return an owned value by move | Allowed |
| Pass a temporary borrow to a function | Allowed |
| Return a borrowed reference | Not allowed |
| Store a borrow somewhere that can outlive its valid context | Not allowed |
| Require the compiler to inspect another function body to prove a borrow safe | Avoided by design |

## 10. Declaration-time type inference

Fast compile supports only simple, local type inference that can be completed when a variable is declared.

A variable's type is fixed at its declaration and does not change later.

```text
let x = 10
```

The compiler determines the type from the initializer and records it immediately. Later uses of `x` do not participate in deciding its type.

The compiler must not infer a local variable's type by searching call sites, other functions, or later statements.

Integer literals use the ordinary `int` type by default unless the declaration explicitly selects another numeric type.

```text
let count = 10        // int
let count: SomeInt = 10
```

The exact set and widths of numeric types are still undecided; `SomeInt` above is only illustrative.

Function parameter and return types are not inferred from usage. Function signatures must contain the information needed by callers without inspecting the function body.

The guiding rule is:

> Type inference may copy a type that is already locally known at declaration time; it must not search for a type.

## 11. Integer defaults and signed integer names

The default ordinary integer type is `int`.

`int` is always a signed 32-bit integer.

```text
let x = 10 // int, signed 32-bit
```

A signed 64-bit integer is named `long`.

```text
let large: long = 8_000_000_000
```

These widths are fixed by the language and do not change with the target platform.

Current mapping:

| Fast compile type | Meaning |
| --- | --- |
| `int` | signed 32-bit integer |
| `long` | signed 64-bit integer |

Names and rules for unsigned integer types are not decided yet.

## 12. Unsigned integers and floating-point types

Unsigned integer types use the following names and fixed widths:

| Fast compile type | Meaning |
| --- | --- |
| `uint` | unsigned 32-bit integer |
| `ulong` | unsigned 64-bit integer |

Floating-point types use the following names and fixed widths:

| Fast compile type | Meaning |
| --- | --- |
| `float` | 32-bit floating-point |
| `double` | 64-bit floating-point |

Together with the previously defined signed integer types, the core numeric names are:

```text
int     // signed 32-bit
long    // signed 64-bit
uint    // unsigned 32-bit
ulong   // unsigned 64-bit
float   // 32-bit floating-point
double  // 64-bit floating-point
```

These widths are fixed by the language and do not vary by target platform.

Floating-point literals default to `float` (32-bit floating-point). A `double` value must be requested explicitly by context or type annotation.

## 13. Core primitive numeric and boolean types

Fast compile uses the following fixed-width primitive types:

| Fast compile type | Meaning |
| --- | --- |
| `sbyte` | signed 8-bit integer |
| `byte` | unsigned 8-bit integer |
| `short` | signed 16-bit integer |
| `ushort` | unsigned 16-bit integer |
| `int` | signed 32-bit integer |
| `uint` | unsigned 32-bit integer |
| `long` | signed 64-bit integer |
| `ulong` | unsigned 64-bit integer |
| `float` | 32-bit floating-point |
| `double` | 64-bit floating-point |
| `bool` | boolean value (`true` or `false`) |

All integer and floating-point widths are fixed by the language and do not change with the target platform.

Default literal types remain:

```text
integer literal        -> int
floating-point literal -> float
```

## 14. Not decided yet

The following areas are still open:

- Integer conversion rules
- Strings
- Arrays and slices
- Struct and enum details
- Error handling
- Generics
- Module/import system
- Build system
- C ABI / FFI
- Raw pointers
- `unsafe`
- Closures
- Threads and async
- Compile-time execution and macros
- Overflow and bounds-checking behavior
- Compiler backend
- File extension
- Final concrete syntax

These should be decided with the primary goal of keeping compilation fast.
