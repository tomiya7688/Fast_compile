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


### Runtime performance is also a core goal

Fast compile is not intended to trade away native runtime performance merely to achieve fast compilation.

The target is:

- extremely fast compilation for large projects;
- native-code execution suitable for performance-sensitive software;
- predictable runtime costs;
- no mandatory garbage collector;
- no mandatory virtual dispatch or hidden object model;
- no exception unwinding machinery;
- no mandatory whole-program optimization for acceptable runtime performance.

The normal lightweight backend should generate reasonably efficient native code with cheap local optimizations.

The optional LLVM backend exists for builds that need more aggressive optimization without forcing that cost onto normal development builds.

Language features should avoid imposing unnecessary hidden runtime costs. When a safety feature does have a runtime cost, such as array bounds checks or integer overflow checks, that cost should be explicit in the language design and kept as small and predictable as possible.

The goal is not merely "compile faster than C++". It is:

> Compile dramatically faster on large projects while remaining a fast native systems language.


### Large-project compilation is the primary target

Fast compile is primarily designed to reduce build times in large codebases where compilation can take many minutes or hours.

Optimizing a tiny project from a few seconds to a slightly smaller number of seconds is not the main value proposition.

Language and compiler design decisions should therefore prioritize:

- avoiding whole-program analysis;
- compiling modules and functions independently where possible;
- minimizing recompilation after local changes;
- maximizing safe parallel compilation;
- preventing generic/template expansion from causing compile-time explosions;
- keeping module interfaces cheap to load without reparsing unrelated source files;
- making build cost scale predictably with the amount of changed code.

A central success criterion is reducing very large build times by a substantial factor, not merely minimizing startup latency on small programs.

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

## 14. No implicit numeric conversions

Fast compile does not perform implicit conversions between numeric types.

Once a value has a numeric type, assigning or passing it to a different numeric type requires an explicit conversion.

```text
let a = 10        // int
let b: long = a   // invalid
let c: long = a as long
```

Mixed-type arithmetic is also not implicitly promoted.

```text
let a: int = 10
let b: long = 20
let c = a + b     // invalid
```

One exception is a numeric literal whose type is determined directly by its declaration context. This is not treated as a conversion from an already-typed value.

```text
let a: long = 10
let b: double = 1.5
```

The compiler only needs to check whether the literal itself is representable in the explicitly requested type.

The guiding rule is:

> Numeric values do not silently change type.

## 15. Strings and character handling

Fast compile has a `string` type but does not have a separate `char` type.

All string literals are `string` values, including literals that visually contain only one character.

```text
let a = "A"      // string
let b = "あ"     // string
let c = "Hello"  // string
```

Strings use UTF-8 encoding.

Indexing a string accesses its underlying UTF-8 bytes and returns a `byte`.

```text
let s = "Hello"
let b = s[0] // byte
```

For multi-byte UTF-8 characters, indexing still operates on bytes rather than Unicode code points or user-perceived characters.

```text
let s = "あ"
let b = s[0] // first UTF-8 byte
```

String length is defined as the number of UTF-8 bytes.

```text
let a = "abc"
a.length // 3

let b = "あ"
b.length // 3
```

Unicode-aware operations such as code-point iteration, grapheme handling, and Unicode substring logic belong in the standard library rather than the core type system.

The guiding rule is:

> Core string operations are byte-oriented; Unicode-aware behavior is explicit.

## 16. Arrays and slices

Fast compile distinguishes owning arrays from borrowed slices.

### Fixed-size arrays

A fixed-size array owns its elements and includes its length in the type.

```text
let values: [int; 4] = [10, 20, 30, 40]
```

`[int; 4]` and `[int; 8]` are different types.

### Dynamic arrays

A dynamic array is an owning container with runtime length.

```text
let values = array<int>()
values.push(10)
values.push(20)
```

Conceptually, a dynamic array contains a pointer, length, and capacity.

Its backing storage is automatically released when ownership ends.

### Borrowed slices

A slice is a non-owning borrowed view written as `&[T]`.

```text
fn sum(values: &[int]) -> int {
    ...
}
```

Conceptually, a slice contains a pointer and length.

Slices follow the same borrowing rule as `&T`:

- they do not own memory;
- they may be used locally or passed temporarily to a function;
- they may not escape into a longer-lived location;
- they may not be returned as borrowed references;
- they may not be stored where they can outlive the borrowed data.

Sub-slices are also borrowed views.

```text
let values = [10, 20, 30, 40]
use(&values[1..3])
```

The guiding rule is:

> Arrays own memory; slices never own memory.

`string` remains a separate type from byte arrays and byte slices. Any conversion between them must be explicit.

## 17. Array and slice bounds checking

Normal array and slice indexing performs bounds checking.

```text
let values = [10, 20, 30]
let x = values[i]
```

At runtime, the index must be within the valid range.

If an index is a compile-time constant and is obviously out of range, compilation should fail immediately.

```text
let values = [10, 20, 30]
let x = values[3] // compile error
```

If the index is only known at runtime, the generated code performs a simple bounds check before accessing memory.

Unchecked indexing is not part of the normal language.

## 18. No unsafe mode in v0.1

Fast compile v0.1 does not have an `unsafe` language mode or `unsafe { ... }` block.

Unchecked array or slice indexing is not available.

Low-level operations such as raw pointers may be designed later if they become necessary, but they will not require the language to introduce a general-purpose mode where normal safety rules are suspended.

The guiding rule is:

> Do not add a dangerous operation until there is a concrete need for it.

## 19. Structs

Fast compile structs are simple value-oriented data structures, broadly similar to Go structs.

There are no classes, inheritance hierarchies, virtual methods, or implicit constructors.

```text
struct User {
    id: int
    name: string
}
```

Struct values are created explicitly with field values. All fields must be initialized; omitted fields are not silently filled with zero/default values.

```text
let user = User {
    id: 1,
    name: "Taro"
}
```

Field types are explicit in the struct definition.

The compiler does not synthesize hidden initialization logic beyond the direct field initialization requested by the source code.

Methods, if supported syntactically, are treated as ordinary functions associated with a struct rather than as a separate object system.

Conceptually:

```text
fn User.print(self: &User) {
    print(self.name)
}
```

may be compiled like an ordinary function taking `&User`.

Ownership rules apply recursively to struct fields.

A struct that contains owning fields does not gain an implicit deep-copy operation. Moving the struct moves ownership of those fields with it.

The guiding rule is:

> A struct is data plus ordinary functions, not a hidden runtime object model.

## 20. Enums

Fast compile enums are simple named integer constants.

Enums do not carry payload values and do not introduce tagged unions or pattern-matching semantics.

```text
enum State {
    Idle = 0
    Running = 1
    Stopped = 2
}
```

Every enum member value must be written explicitly. The compiler does not automatically assign sequential values.

The underlying representation is `int` (signed 32-bit).

Conceptually:

```text
State.Idle    == 0
State.Running == 1
State.Stopped == 2
```

More complex sum types or tagged unions may be designed separately in the future if needed, rather than making `enum` itself more complex.

The guiding rule is:

> An enum is a named set of explicit `int` constants.

## 21. Error handling

Fast compile does not have exceptions.

There is no `throw`, `try`, `catch`, stack unwinding, or recoverable panic mechanism.

Recoverable failures are returned explicitly with the built-in `Result<T, E>` type.

```text
fn parse(text: string) -> Result<int, ParseError> {
    ...
}
```

`Result<T, E>` has exactly two states:

```text
Ok(T)
Err(E)
```

It is a built-in language type rather than a general user-defined payload enum.

This allows error handling without introducing general tagged unions or exception control flow.

A caller handles a result through three built-in members:

- `.ok` returns a `bool`;
- `.value` returns the success value;
- `.error` returns the error value.

```text
let result = parse("123")

if result.ok {
    let value = result.value
} else {
    let error = result.error
}
```

There is no general pattern-matching requirement for `Result`.

Accessing `.value` while the result contains `Err`, or accessing `.error` while it contains `Ok`, is a runtime programming error and terminates the program. The compiler does not need branch-sensitive analysis to prove that the correct member is accessed.

`Ok(value)` and `Err(error)` are the built-in constructors.

Convenience propagation syntax such as `?` is not part of v0.1.

Ownership applies only to the active value inside the result. When a `Result` is moved or destroyed, only its active `T` or `E` value is moved or destroyed.

Programming errors that are not intended to be recovered from, such as a runtime array bounds failure, terminate the program rather than throwing an exception.

The guiding rule is:

> Expected failure is an explicit value. Programming errors are not exceptions.

## 22. Modules, compilation units, and incremental builds

Fast compile treats large-project build scalability as a core language/toolchain concern.

### Files and modules

A source file is an independently compilable source unit.

A module is a namespace and dependency boundary that may contain multiple source files.

Changing one source file must not require reparsing or recompiling every other source file in the same module unless their actual dependencies changed.

The exact source syntax for declaring and importing modules is still provisional.

### Explicit public API

Only explicitly exported declarations are part of a module's public interface.

The exact keyword is provisional; examples may use `pub`.

Keeping a declaration private means changes to it cannot invalidate external modules merely because its implementation changed.

### Compiled interface metadata

The compiler emits compact machine-readable interface metadata for exported declarations.

Other modules read this metadata instead of reparsing dependency source files.

The interface contains only information required by callers, such as:

- exported names;
- function parameter and return types;
- exported struct type/layout information when required;
- enum names and values;
- exported constants and other compile-time values when required.

Ordinary function bodies are not part of the public interface.

The interface format should be binary or otherwise directly loadable by the compiler; it is not intended to behave like a C/C++ header file.

### Per-symbol dependency tracking

Incremental dependency tracking is performed at exported-symbol granularity rather than only at module granularity.

When compiling code, the compiler records the exact external symbols and type layouts that code depends on, together with stable fingerprints of those interfaces.

If an unrelated exported symbol changes, code that never depended on it does not need to be recompiled.

For example:

```text
module A exports:
    foo(int) -> int
    bar(string) -> int

module B uses only A.foo
```

Changing the implementation of `A.bar`, or even changing `A.bar`'s public signature, does not by itself require recompiling module B.

Changing the public signature of `A.foo` does.

### Implementation changes do not invalidate callers

If a function body changes but its public interface fingerprint does not change, callers are not recompiled.

Only the changed implementation needs new code generation, followed by the necessary link/update step.

This rule intentionally discourages compilation strategies that require callers to inspect callee bodies.

### Function-level incremental cache

Within a changed source file, function bodies are independently cacheable.

After the file is parsed, each function can be checked and code-generated using a cache key derived from at least:

- the function body;
- its signature;
- the external interface fingerprints it actually uses;
- relevant compiler options;
- the target platform/backend version.

Unchanged functions whose cache keys are still valid may reuse their previously generated result.

This means editing one function in a very large file need not force expensive code generation for every other function in that file.

Parsing the changed file itself may still occur; parsing is expected to remain deliberately cheap.

### Parallel compilation

Independent source files and independent function bodies should be compilable in parallel once the interfaces they require are available.

Module dependencies should form a directed acyclic graph. Circular module imports are not supported.

Recursive function calls within an already-resolved module/interface are still allowed; the restriction is on module dependency cycles.

### No mandatory whole-program analysis

Normal builds do not require whole-program type checking, cross-module lifetime analysis, or cross-module function-body inspection.

Cross-module inlining and whole-program/LTO-style optimization are not required for a normal Fast compile build and must not be necessary for correctness.

A future optional slow optimization mode may exist, but it must remain separate from the normal fast build path.

### Build optimization priorities

Fast compile optimizes build time at two levels:

1. **Avoid work:** do not recompile code whose relevant interfaces and implementation cache keys did not change.
2. **Make remaining work cheap:** keep lexing, parsing, type checking, dependency lookup, and code generation simple and efficient.

Small constant-factor compiler optimizations still matter because they are multiplied across very large codebases, but avoiding unnecessary work has priority.

The guiding rule is:

> A local implementation change should cause a local rebuild unless it actually changes an interface that other code depends on.

## 23. Generics

Fast compile supports user-defined generics, but the normal fast build does not monomorphize and recompile generic bodies for every concrete type.

### Compile generic bodies once

A generic function body is type-checked once against opaque type parameters and code-generated once for the normal build.

```text
fn identity<T>(value: T) -> T {
    return move value
}
```

Calls such as:

```text
identity(10)
identity("hello")
```

must not cause two independent compilations of the function body.

The same principle applies to methods on generic structs.

### Opaque type parameters

A type parameter such as `T` is treated as an opaque type inside generic code.

Generic code may perform operations that are valid without knowing the concrete type, including:

- move a value;
- borrow a value;
- store and load a value;
- pass it to another compatible generic or typed function;
- return it;
- place it in a generic struct or built-in generic container.

Generic code may not assume that an arbitrary `T` supports arithmetic, comparison, methods, or other type-specific operations.

For example:

```text
fn add<T>(a: T, b: T) -> T {
    return a + b // invalid: + is not defined for arbitrary T
}
```

Fast compile v0.1 does not introduce traits, concepts, generic constraints, specialization, or template metaprogramming to make such operations valid.

### Runtime type descriptors

The normal generic calling convention may pass compact hidden type descriptors for type parameters.

A descriptor contains only low-level information required to manipulate an opaque value, such as:

- size;
- alignment;
- move behavior;
- destruction behavior;
- layout information required by a generic aggregate.

This allows one compiled generic body to operate on multiple concrete types without recompiling that body.

Creating or looking up a descriptor for `Box<int>` or `Box<string>` is not considered recompiling the generic implementation.

### Generic structs

User-defined generic structs are allowed.

```text
struct Box<T> {
    value: T
}
```

Concrete layout information for a generic struct may be computed from the type descriptors of its fields.

This layout computation should be small, cacheable metadata work rather than a fresh parse/type-check/codegen pass over the generic source.

### Generic type inference

Generic type arguments may be inferred only from the direct argument types at the call site.

```text
identity(10)      // T = int
identity("hello") // T = string
```

The compiler does not infer generic types from return-value context, distant call sites, implicit numeric conversions, or whole-program analysis.

Explicit type arguments remain valid when needed.

### No template-style expansion

Normal Fast compile builds do not:

- paste generic source into each caller;
- reparse generic source for each concrete type;
- type-check the same generic body once per concrete type;
- generate an unbounded number of native implementations through recursive template expansion.

An optional future slow optimization mode may specialize selected generic functions for runtime performance, but specialization must not be required for correctness or for the normal fast build.

The guiding rule is:

> A generic abstraction may be instantiated many times, but its source body should be compiled once.

## 24. C ABI, FFI, and opaque pointers

Fast compile v0.1 supports interoperability through the C ABI.

Fast compile does not directly implement the C++ ABI.

The exact declaration syntax is provisional; examples may use `extern "C"`.

```text
extern "C" fn create_window() -> ptr<Window>
extern "C" fn destroy_window(window: ptr<Window>)
```

### C ABI only

The core language interoperates with stable C-compatible function boundaries.

It does not directly model C++ ABI features such as:

- C++ name mangling;
- classes and virtual dispatch;
- C++ templates;
- C++ exceptions;
- RTTI;
- compiler-specific C++ object ABI details.

A C++ library can be exposed through a C-compatible wrapper.

Fast compile intends to provide its own C++ wrapper/adaptor tooling later so users do not have to design that bridge manually for every project. That wrapper layer is separate from the core language and does not make the compiler itself implement the C++ ABI.

### Opaque pointer type

Fast compile has an opaque pointer type written as `ptr<T>` for FFI and other low-level handles.

A `ptr<T>` may be stored, compared where appropriate, passed to functions, and returned from functions.

Normal Fast compile code cannot directly dereference a `ptr<T>`.

```text
let window = create_window()

// no ordinary *window-style dereference

destroy_window(window)
```

This keeps raw foreign addresses from bypassing the normal ownership and borrowing model.

Fast compile v0.1 does not add an `unsafe` block merely to make `ptr<T>` dereferenceable.

### FFI-compatible values

The initial C FFI permits simple fixed-width scalar values and opaque pointers.

The core set includes the fixed-width Fast compile numeric types and `ptr<T>`.

ABI mapping is based on the actual width and calling convention, not on similarly named C source types. For example, Fast compile `long` is always 64-bit even though C/C++ `long` varies by platform.

Fast compile-specific runtime types are not passed across the C ABI implicitly.

Examples include:

- `string`;
- `array<T>`;
- `Result<T, E>`;
- ordinary generic structs.

Such values must be converted explicitly to a C-compatible representation, for example pointer + length for byte-oriented data.

C-compatible struct layout support may be added separately when needed; ordinary Fast compile structs do not silently become C-layout structs.

The guiding rule is:

> Foreign code crosses a small explicit C ABI boundary; C++ complexity stays outside the core compiler.

## 25. Compiler implementation and code-generation backends

Fast compile uses two code-generation paths with different priorities.

### Default lightweight backend

The normal Fast compile build uses a lightweight native backend designed primarily for compilation throughput.

The default backend should:

- consume Fast compile's own simple IR;
- perform only cheap, predictable lowering and local optimization;
- emit native object code without requiring LLVM;
- avoid expensive whole-program optimization;
- be suitable for highly parallel function-level code generation;
- keep startup and per-function code-generation overhead small.

The normal fast build must remain fully functional when LLVM is not installed.

### Optional LLVM backend

LLVM is supported as an optional backend for builds where generated-code quality matters more than compile time.

Conceptually:

```text
source
  -> Fast compile front end
  -> Fast compile IR
       -> lightweight native backend   // normal fast build
       -> LLVM backend                 // slower optimized build
```

Both backends share the same parser, type system, ownership checks, module metadata, and incremental dependency model.

LLVM is not part of language correctness and must not become a mandatory dependency of the normal compiler path.

A build mode may explicitly select LLVM or another future optimization backend.

### Compiler implementation language

The initial Fast compile compiler is implemented in C.

C is preferred over Fortran for the compiler implementation because the workload is dominated by systems-oriented operations such as parsing, byte processing, hash tables, file and object-format handling, memory management, and operating-system interfaces rather than large numerical kernels.

The compiler implementation should avoid unnecessary runtime dependencies and keep its own execution overhead low.

A future self-hosted compiler may be considered later, but self-hosting is not required for the initial implementation.

### Assembly only for partial, measured optimization

The compiler is written primarily in C and should normally rely on a high-quality C compiler's optimizer.

Handwritten assembly is used only as a partial optimization technique for isolated hot paths where profiling demonstrates a real benefit.

Fast compile does not treat assembly as a general replacement for C. In many cases, compiler-generated machine code is preferable to manually written assembly because it can optimize more effectively across surrounding code and target-specific details.

Any assembly optimization should therefore:

- be limited to a small, well-defined routine or code path;
- be justified by measurement;
- have a portable C fallback;
- preserve a clear C reference implementation where practical;
- never become required for the compiler's overall architecture.

Likely candidates include extremely hot byte-scanning, hashing, copying, or similarly tight low-level loops, but only after the C implementation and compiler optimization have been measured.

The guiding rule is:

> Write the compiler in C, trust the C optimizer by default, and use assembly only as a measured local optimization.

## 26. Initial target architecture and platform order

The first native target architecture for Fast compile is x86-64.

Platform support is developed in this order:

1. Linux x86-64
2. Windows x86-64
3. Additional platforms and architectures later

The initial lightweight backend should focus on producing correct x86-64 machine code efficiently before additional architectures are added.

The first platform implementation targets Linux. Windows support follows after the Linux backend and toolchain path are working reliably.

ARM64 and other architectures may be added later, but they are not required for the first implementation.

The exact object-file format emission, executable generation strategy, and linker integration for each platform are still separate design decisions.

## 27. Linux linker strategy

Fast compile initially emits ELF object files (`.o`) on Linux x86-64 and delegates final linking to an external high-speed linker.

The default linker is **mold**.

The initial preference order is:

1. `mold`
2. `ld.lld` as the primary fallback
3. the system linker only as a compatibility fallback

### Why mold

The default choice prioritizes link throughput for very large builds.

mold is specifically designed as a high-performance ELF linker and supports Linux x86-64.

Fast compile does not depend on mold for language correctness; it is the preferred external linker for the normal fast-build path.

### Why keep lld support

LLD remains an important fallback because it is mature, fast, widely deployed, and supports both ELF and PE/COFF.

Keeping LLD compatibility also provides a useful alternate path for environments where mold is unavailable or incompatible with a particular link requirement.

### Object-file strategy

The lightweight backend initially emits ordinary ELF object files rather than generating the final executable directly.

This keeps the first implementation smaller and lets Fast compile benefit immediately from mature parallel linkers.

A future Fast compile-native incremental linker may bypass some object-file write/read work and may directly update or emit final executables, but that is a later optimization and is not required for v0.1.

The guiding rule is:

> Use mold by default, keep LLD as a fast fallback, and postpone a custom linker until measurements justify the implementation cost.

## 28. Integer overflow behavior

Normal integer arithmetic is checked for overflow.

If an overflow is provably visible at compile time, compilation fails.

```text
let x: byte = 300 // compile error
```

If overflow depends on runtime values, the generated code checks the operation and terminates the program if the result cannot be represented in the destination type.

```text
let c = a + b // runtime overflow check when needed
```

This applies to signed and unsigned integer types.

Fast compile does not silently wrap integer arithmetic in normal code.

Explicit wrapping operations may be added later if low-level code requires them, but they are not part of the default arithmetic semantics.

The guiding rule is:

> Normal integer arithmetic either produces a representable result or terminates; silent wraparound is not allowed.

## 29. Not decided yet

The following areas are still open:

- Closures
- Threads and async
- Compile-time execution and macros
- Windows object/executable format and linker strategy
- Linker strategy
- File extension
- Final concrete syntax
- C-compatible struct layout details
- C++ wrapper/adaptor tooling details

These should be decided with the primary goal of keeping compilation fast.
