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


### Small runtime footprint is a core goal

Fast compile also targets a small runtime footprint.

Large codebases may run on systems where CPU and memory resources are limited, including embedded, automotive, industrial, and other resource-constrained environments.

The language must not require a large always-resident runtime merely to execute ordinary compiled code.

The core runtime should therefore remain minimal and should avoid mandatory subsystems such as:

- a garbage collector;
- exception unwinding;
- reflection metadata;
- a virtual machine;
- a large object runtime;
- mandatory background threads;
- mandatory async executors;
- hidden global allocation registries;
- large per-type runtime metadata.

Ordinary code should compile as directly as practical to native code and ordinary calls.

Features that need runtime support should use small, explicit helpers rather than pulling in a large monolithic runtime.

The standard library and the language runtime are separate concerns. Programs should not be forced to link unused standard-library functionality.

A minimal program should be able to link only the small runtime pieces it actually requires.

Generic type descriptors, bounds-failure handling, overflow-failure handling, allocation helpers, and similar support mechanisms should be compact and linkable on demand.

A future freestanding or restricted-runtime profile should be possible without redesigning the language.

The guiding rule is:

> A large source tree must not imply a large runtime footprint.


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


### Correctness reduces total build time

Fast compile treats compiler correctness and clear diagnostics as part of build performance.

A compiler that finishes quickly but frequently miscompiles code, crashes, invalidates caches incorrectly, or reports errors poorly causes developers to rebuild repeatedly and loses the time saved by fast compilation.

The compiler and language should therefore prioritize:

- deterministic compilation;
- conservative, correct incremental-cache invalidation;
- precise local diagnostics;
- avoiding undefined or ambiguous language behavior where practical;
- strong validation of compiler-generated metadata and object files;
- reproducible tests for parser, type checker, ownership rules, incremental builds, and code generation;
- differential testing between the lightweight backend and LLVM where possible.

The fast path must not depend on skipping correctness checks that are required for valid compilation.

The guiding rule is:

> The fastest build is the build that is both quick and correct the first time.


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
const data = new Data()
```

Here, `data` owns the allocated value.

When the owner leaves its scope, the value is automatically destroyed/freed.

```text
fn example() {
    const data = new Data()
    use(&data)
} // data is automatically released here
```

This also applies to early returns and other normal exits from the scope.

## 4. Explicit ownership transfer

Ownership transfer is explicit.

```text
const a = new Data()
const b = move a

use(b) // OK
use(a) // invalid: a no longer owns a value
```

After a move, the source variable is no longer usable as an owned value.

An owned value may cross a function boundary by moving ownership.

```text
fn create() -> Data {
    const value = new Data()
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
const data = new Data()
show(&data)
use(data) // still owned here
```

## 6. Borrowed references do not escape

Fast compile does not attempt a general-purpose lifetime proof across functions.

A borrowed reference is valid only as a temporary view within the permitted local/function-call context. It cannot escape into a longer-lived location.

For example, returning a borrowed reference is not supported:

```text
fn bad() -> &Data {
    const data = new Data()
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
const data = new Data()
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
const x = 10
```

The compiler determines the type from the initializer and records it immediately. Later uses of `x` do not participate in deciding its type.

The compiler must not infer a local variable's type by searching call sites, other functions, or later statements.

Integer literals use the ordinary `int` type by default unless the declaration explicitly selects another numeric type.

```text
const count = 10        // int
const count: SomeInt = 10
```

The exact set and widths of numeric types are still undecided; `SomeInt` above is only illustrative.

Function parameter and return types are not inferred from usage. Function signatures must contain the information needed by callers without inspecting the function body.

The guiding rule is:

> Type inference may copy a type that is already locally known at declaration time; it must not search for a type.

## 11. Integer defaults and signed integer names

The default ordinary integer type is `int`.

`int` is always a signed 32-bit integer.

```text
const x = 10 // int, signed 32-bit
```

A signed 64-bit integer is named `long`.

```text
const large: long = 8_000_000_000
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
const a = 10        // int
const b: long = a   // invalid
const c: long = a as long
```

Mixed-type arithmetic is also not implicitly promoted.

```text
const a: int = 10
const b: long = 20
const c = a + b     // invalid
```

One exception is a numeric literal whose type is determined directly by its declaration context. This is not treated as a conversion from an already-typed value.

```text
const a: long = 10
const b: double = 1.5
```

The compiler only needs to check whether the literal itself is representable in the explicitly requested type.

The guiding rule is:

> Numeric values do not silently change type.

## 15. Strings and character handling

Fast compile has a `string` type but does not have a separate `char` type.

All string literals are `string` values, including literals that visually contain only one character.

```text
const a = "A"      // string
const b = "あ"     // string
const c = "Hello"  // string
```

Strings use UTF-8 encoding.

Indexing a string accesses its underlying UTF-8 bytes and returns a `byte`.

```text
const s = "Hello"
const b = s[0] // byte
```

For multi-byte UTF-8 characters, indexing still operates on bytes rather than Unicode code points or user-perceived characters.

```text
const s = "あ"
const b = s[0] // first UTF-8 byte
```

String length is defined as the number of UTF-8 bytes.

```text
const a = "abc"
a.length // 3

const b = "あ"
b.length // 3
```

Unicode-aware operations such as code-point iteration, grapheme handling, and Unicode substring logic belong in the standard library rather than the core type system.

The guiding rule is:

> Core string operations are byte-oriented; Unicode-aware behavior is explicit.


### String ownership

`string` is a move-only type.

A string literal refers to UTF-8 bytes stored in static program data and does not require heap allocation.

```text
const name = "Fast compile"
```

The value is still a `string` and still follows move-only rules; the only difference is that its backing storage is marked as static, so destruction does not free those bytes.

A string created dynamically, such as by concatenation or a standard-library operation, owns heap storage and releases that storage when ownership ends.

The string representation is fixed for 64-bit targets as two machine words:

```text
data pointer       // 64 bits
tagged byte length // 64 bits
```

The most-significant bit of the length word is the ownership bit.

```text
ownership bit = 0 -> backing bytes are not heap-owned by this string
ownership bit = 1 -> backing bytes are heap-owned and must be freed on destruction
remaining 63 bits -> UTF-8 byte length
```

Therefore a `string` is 16 bytes on x86-64 and other 64-bit targets using this representation.

A string literal points directly into immutable static program data and has ownership bit 0.

A dynamically created string points to allocator-owned memory and has ownership bit 1.

On destruction:

```text
if ownership bit == 1:
    fc_free(data pointer)
else:
    do nothing
```

The maximum string length represented by this layout is `2^63 - 1` bytes.

String contents are immutable. A `var string` permits rebinding the variable to another string value; it does not make the referenced UTF-8 bytes mutable. Operations such as concatenation create a new string value.

Because strings are immutable, the core `string` representation does not store capacity. Builders or mutable byte buffers, when needed, belong in separate standard-library types such as dynamic byte arrays.

There is no reference counting and no implicit copy of string data.

```text
const a = make_string()
const b = a      // compile error
const c = move a // allowed
```

String literals may be reused or deduplicated by the compiler because their bytes are immutable static data.

The guiding rule for ownership is:

> Every `string` obeys the same move-only semantics; only the backing storage's destruction behavior differs.

## 16. Arrays and slices

Fast compile distinguishes owning arrays from borrowed slices.

### Fixed-size arrays

A fixed-size array owns its elements and includes its length in the type.

```text
const values: [int; 4] = [10, 20, 30, 40]
```

`[int; 4]` and `[int; 8]` are different types.

### Dynamic arrays

A dynamic array is an owning container with runtime length.

```text
var values = array<int>()
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
const values = [10, 20, 30, 40]
use(&values[1..3])
```

The guiding rule is:

> Arrays own memory; slices never own memory.

`string` remains a separate type from byte arrays and byte slices. Any conversion between them must be explicit.

## 17. Array and slice bounds checking

Normal array and slice indexing performs bounds checking.

```text
const values = [10, 20, 30]
const x = values[i]
```

At runtime, the index must be within the valid range.

If an index is a compile-time constant and is obviously out of range, compilation should fail immediately.

```text
const values = [10, 20, 30]
const x = values[3] // compile error
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

Struct values are created explicitly with field values. Every field must be initialized either by the constructor expression or by an explicit default declared on that field; omitted fields are never silently filled with an undeclared zero/default value.

```text
const user = User {
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
const result = parse("123")

if result.ok {
    const value = result.value
} else {
    const error = result.error
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

### Packages, file modules, and compilation units

A `.fscm` source file is an independently compilable source unit and a file module.

A directory is a package and is the unit named by `import`.

For example:

```text
server/
├─ post.fscm
├─ user.fscm
└─ config.fscm
```

defines package `server` with file modules:

```text
server.post
server.user
server.config
```

External source code imports the package:

```text
import server
```

and names a public symbol by package, file module, and symbol:

```text
server.post::letter
server.user::find
server.config::MaxUsers
```

The file remains the compilation unit. Importing a package does not mean all source files in that directory are reparsed or recompiled.

Package/interface metadata provides a compact index of available file modules and public symbols, while actual dependency tracking remains at file/symbol granularity.

A nested directory is a separate package. Importing `server` does not implicitly import `server.admin` or other child packages.

Changing one source file affects only that file module and the external symbols that actually depend on its changed public interface.

### Explicit public API

Only explicitly exported declarations are part of a module's public interface.

The exact keyword is provisional; examples may use `public`.

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

### Opaque pointer type and null

Fast compile has one opaque pointer type written as `ptr<T>`.

`ptr<T>` is intended for FFI and other low-level handles.

A `ptr<T>` may contain a null pointer value.

```text
extern "C" fn create_window() -> ptr<Window>

const window = create_window()

if window == null {
    ...
}
```

A `ptr<T>` may be stored, compared, passed to functions, and returned from functions.

Normal Fast compile code cannot directly dereference a `ptr<T>`.

```text
window.field   // invalid
*window        // invalid
```

Because `ptr<T>` is not dereferenceable, a null value inside `ptr<T>` cannot cause a null pointer dereference in Fast compile code.

Normal references and owned values remain non-null by definition.

Fast compile v0.1 does not add an `unsafe` block merely to make `ptr<T>` dereferenceable.

The guiding rule is:

> Null is allowed only in opaque pointer handles that Fast compile itself cannot dereference.

### FFI-compatible values

The initial C FFI permits simple fixed-width scalar values and opaque pointers.

The core set includes the fixed-width Fast compile numeric types and opaque `ptr<T>` values.

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
const x: byte = 300 // compile error
```

If overflow depends on runtime values, the generated code checks the operation and terminates the program if the result cannot be represented in the destination type.

```text
const c = a + b // runtime overflow check when needed
```

This applies to signed and unsigned integer types.

Fast compile does not silently wrap integer arithmetic in normal code.

Explicit wrapping operations may be added later if low-level code requires them, but they are not part of the default arithmetic semantics.

The guiding rule is:

> Normal integer arithmetic either produces a representable result or terminates; silent wraparound is not allowed.

## 29. Allocator model

Fast compile does not require a large built-in allocator implementation.

The language/runtime defines only a minimal allocation ABI. The allocator implementation is supplied by the platform or selected build configuration.

Conceptually, the required operations are equivalent to:

```text
alloc(size)
realloc(pointer, size)
free(pointer)
```

### Default Linux allocator

The initial Linux implementation uses the system allocator underneath the Fast compile allocation ABI.

Conceptually:

```text
fc_alloc   -> malloc
fc_realloc -> realloc
fc_free    -> free
```

The language is not permanently tied to these functions; they are only the initial platform implementation.

### Replaceable allocator

A build may replace the allocator implementation without changing application source code.

This allows future configurations such as:

- a custom high-performance allocator;
- a fixed-pool allocator;
- an application-specific allocator;
- a restricted embedded allocator;
- a no-heap profile.

The exact build syntax for selecting an allocator is still provisional.

### No per-container allocator object

Ordinary containers do not carry an allocator object or allocator function pointer per instance.

For example, a dynamic array remains conceptually:

```text
pointer
length
capacity
```

rather than:

```text
pointer
length
capacity
allocator-pointer
```

Allocator selection is normally a program/build-level decision. This avoids increasing every container's memory footprint and avoids mandatory allocator dispatch on every allocation.

### Future no-heap profile

The language design should permit a future no-heap build profile.

In such a profile, operations that require dynamic allocation are rejected, while fixed-size stack/static data remains usable.

This is intended for embedded, automotive, and other restricted environments.

The guiding rule is:

> Define a tiny allocation boundary, keep allocator policy replaceable, and do not impose allocator metadata on every object.

## 30. Closures

Fast compile supports closures, but closure capture is explicit and designed to avoid escape analysis and hidden heap allocation.

The exact closure syntax is provisional.

### No implicit capture

A closure never scans its surrounding scope and silently captures referenced variables.

Every captured value must be listed explicitly.

Conceptually:

```text
const factor = 2

const scale = [factor](x: int) -> int {
    return x * factor
}
```

The compiler therefore knows the complete closure environment directly from the closure declaration.

### Closure environment is an ordinary value

A capturing closure is lowered to a small anonymous value containing only its explicitly captured fields plus an associated generated invoke function.

Conceptually:

```text
closure:
    captured fields

generated function:
    invoke(environment, arguments...)
```

The closure environment is not automatically heap allocated.

If a closure is local, its captured storage may remain local just like an ordinary struct.

### Capture ownership

Capturing a trivially copyable scalar value such as an integer, floating-point value, boolean, enum, or opaque pointer copies that value into the closure environment.

Capturing an owning value requires explicit `move`.

Conceptually:

```text
const data = array<int>()

const work = [move data]() {
    ...
}
```

After the move, the outer `data` no longer owns the array.

Borrowed references such as `&T` and `&[T]` cannot be stored as closure captures in v0.1.

This avoids introducing closure-specific lifetime inference.

### Borrowed callable interface

A function may accept a closure through a borrowed callable interface.

The exact type syntax is provisional; examples use `&fn(...)`.

```text
fn for_each(values: &[int], action: &fn(int) -> void) {
    ...
}

const factor = 2

const scale = [factor](x: int) {
    print(x * factor)
}

for_each(&values, &scale)
```

A borrowed callable is conceptually passed as:

```text
invoke-function pointer
environment pointer
```

The environment pointer is valid only for the duration allowed by the normal borrowing rules.

A borrowed callable cannot be stored or returned in a way that escapes its owner.

This provides callbacks without requiring a garbage collector, reference counting, escape analysis, or hidden closure allocation.

### Non-capturing closures

A closure with no captures has no environment and can be lowered directly to an ordinary function pointer where compatible.

Named functions may also be used where a compatible borrowed callable is expected.

### No general closure runtime

Fast compile does not require a global closure registry, reference counting, garbage collection, or a mandatory heap-backed function object.

If a future feature needs a long-lived dynamically erased callable, it should be designed explicitly rather than making every closure pay that cost.

The guiding rule is:

> Capture explicitly, store captures as ordinary data, and borrow closures across call boundaries instead of hiding allocation or lifetime analysis.

## 31. Threads and async

Fast compile v0.1 supports native operating-system threads.

`async` / `await` is not part of v0.1.

### Native threads, not a language runtime scheduler

Thread support is a thin standard-library/runtime wrapper over platform-native threads.

The language does not require:

- a green-thread scheduler;
- a global task runtime;
- a background worker pool;
- an async executor;
- a garbage collector;
- a mandatory event loop.

On Linux, the initial implementation may use the platform's normal pthread-compatible thread facilities. Windows uses the corresponding native thread facilities when that target is added.

### Spawning transfers ownership

A spawned thread must own everything it needs after the spawn call returns.

Conceptually:

```text
const data = array<int>()

const worker = [move data]() {
    process(data)
}

const thread = thread.spawn(move worker)
```

Borrowed references such as `&T` and `&[T]` cannot be captured by a spawned thread.

This means the compiler does not need cross-thread lifetime analysis to prove that stack references remain valid.

The closure environment is moved into thread-owned storage required by the platform thread start mechanism and is destroyed when the thread function finishes.

### Thread handles

`thread.spawn` returns a small thread handle.

A caller may explicitly wait for completion with `join`.

Dropping a thread handle does not stop the running thread; it releases/detaches the handle according to the platform wrapper semantics.

v0.1 does not require implicit joining at scope exit.

### Synchronization

Low-level synchronization primitives such as mutexes, condition variables, and atomic operations belong in the standard library/platform layer.

They should be thin wrappers or compiler intrinsics where appropriate and must not pull in a large scheduler/runtime.

Higher-level concurrency structures may be added later as libraries rather than mandatory language runtime features.

### No async/await in v0.1

Fast compile v0.1 does not transform functions into async state machines and does not provide built-in `async` / `await`.

This keeps normal compilation, code size, runtime requirements, and control flow simple.

Async support may be designed later if there is a demonstrated need. Any future design should avoid making a global executor or large runtime mandatory for programs that do not use async.

The guiding rule is:

> Use explicit native threads first; do not make every program pay for an async runtime.

## 32. Macros and compile-time execution

Fast compile v0.1 does not include a general-purpose macro system.

It also does not execute arbitrary user code during compilation.

This means v0.1 has no:

- textual preprocessor macros;
- AST/procedural macros;
- template metaprogramming;
- arbitrary compile-time functions;
- user code that runs inside the compiler process.

### Why macros are excluded initially

The language is primarily designed for very large projects, where macro systems can increase build cost and make dependencies harder to understand and cache.

A source file should be understandable from its explicit imports, declarations, and ordinary code rather than depending on hidden source transformation.

Avoiding macros also keeps parsing, diagnostics, IDE tooling, dependency tracking, and incremental compilation more predictable.

### Compile-time constants remain simple

Simple constants and constant expressions may still be evaluated by the compiler when all inputs are already known locally.

For example, fixed array lengths, enum values, numeric literals, and other small deterministic expressions do not require a general compile-time programming system.

This constant evaluation must remain bounded, local, and cheap.

### Code generation belongs outside the language

Projects that genuinely need generated source may use an external code-generation step that produces ordinary Fast compile source or interface data.

Generated output is then compiled and cached like normal source.

This keeps code generation visible to the build graph instead of embedding an open-ended programming environment inside every compilation.

A future declarative macro facility may be considered only if large real-world projects demonstrate a clear need and if it can preserve predictable incremental compilation.

The guiding rule is:

> Prefer ordinary functions, generics, and external code generation over hidden compile-time source transformation.

## 33. Source file extension

Fast compile source files use the `.fscm` file extension.

Examples:

```text
main.fscm
math.fscm
network.fscm
engine.fscm
```

The extension is part of the normal source-file convention for the language.

## 34. Mutability

Fast compile uses two variable-declaration forms:

- `const` creates an immutable binding;
- `var` creates a mutable binding.

There is no `let` declaration form.

```text
const x = 10
x = 20 // compile error

var y = 10
y = 20 // allowed
```

Type inference and explicit types work with both forms.

```text
const x = 10        // inferred int, immutable
const x2: int = 10  // explicit int, immutable

var y = 10          // inferred int, mutable
var y2: int = 10    // explicit int, mutable
```

A variable's type is still fixed at declaration time.

```text
var x = 10   // int
x = 20       // allowed
x = 1.5      // compile error: float is not int
```

Mutating operations on owned values require a mutable binding.

```text
var values = array<int>()
values.push(10)
```

An immutable binding cannot be modified through ordinary language operations.

```text
const values = array<int>()
values.push(10) // compile error
```

Function parameters are immutable by default. A parameter that must be rebound or mutated inside the function is declared with `var`.

### `const` is not a compile-time-only declaration

A `const` value may be initialized from a runtime expression.

```text
const value: int = read_value()
```

This is valid because `const` means only that the binding cannot be changed after initialization.

When a language feature requires a compile-time value, the compiler checks whether the expression is locally evaluable at compile time.

```text
const size = 1024
var buffer: [byte; size] // allowed if size is a compile-time-evaluable expression
```

```text
const size = read_size()
var buffer: [byte; size] // compile error: size is not compile-time evaluable
```

Fast compile does not introduce a separate `constexpr`-style declaration in v0.1.

The guiding rule is:

> Use `const` for values that do not change and `var` for values that do.


## 35. Copy and move semantics

Fast compile divides values into two categories:

- copyable values;
- move-only owning values.

### Copyable primitive values

The following primitive values are copyable:

- `sbyte`
- `byte`
- `short`
- `ushort`
- `int`
- `uint`
- `long`
- `ulong`
- `float`
- `double`
- `bool`
- enum values
- `ptr<T>`

Assigning or passing one of these values copies the value.

```text
const a: int = 10
const b = a // copy
```

### Owning values are move-only

Types that own resources are not copied implicitly.

Examples include dynamically allocated arrays and owned strings.

```text
var data = array<int>()

const a = data
// compile error: array<int> is move-only

const b = move data
// allowed
```

After an explicit move, the source value is no longer usable as an owner.

### Struct copyability is structural

A struct is copyable only if every field is copyable.

```text
struct Point {
    x: int
    y: int
}
```

`Point` is copyable.

```text
struct User {
    id: int
    name: string
}
```

`User` is move-only because it contains an owning `string`.

The same structural rule applies recursively to fixed-size arrays, generic structs, and built-in generic containers where applicable.

### Fixed-size arrays

A fixed-size array is copyable only if its element type is copyable.

```text
[int; 100]     // copyable
[string; 100]  // move-only
```

### Result

`Result<T, E>` is copyable only if both `T` and `E` are copyable.

Otherwise it is move-only.

### Function calls

Passing a copyable value by value copies it.

```text
fn use_number(x: int) {
    ...
}

const n = 10
use_number(n) // copy
```

Passing a move-only value by value requires an explicit `move`.

```text
fn consume(values: array<int>) {
    ...
}

var values = array<int>()

consume(values)      // compile error
consume(move values) // allowed
```

Borrowing remains the non-owning alternative when ownership should not move.

The guiding rule is:

> If every component is copyable, the value is copyable. If any component owns a resource, the value is move-only.


## 36. Null safety

Fast compile has no general nullable form of ordinary language values.

The nullability rules are:

```text
&T          // never null
&[T]        // never null
string      // never null
array<T>    // never null
ptr<T>      // may be null, but cannot be dereferenced
```

There is no separate nullable-reference type in v0.1.

Because the only null-capable pointer type is opaque and non-dereferenceable, Fast compile code cannot perform a null pointer dereference through the normal language.

Foreign C code may still fail internally if an API is misused; that remains outside Fast compile's own pointer-safety guarantee.

The guiding rule is:

> Ordinary values are non-null; opaque foreign pointers may be null but are not dereferenceable.


## 37. No mutable borrows

Fast compile v0.1 does not have mutable borrowed references such as `&var T` or `&mut T`.

A borrowed reference `&T` is always read-only.

If a function needs to transform an owned value, ownership is moved into the function and the transformed value is returned to the caller.

```text
fn normalize(var data: Data) -> Data {
    // data is owned here and may be modified locally
    ...
    return move data
}

var data = load()
data = normalize(move data)
```

The caller therefore makes mutation of its own binding explicit through assignment.

A function cannot directly mutate an ordinary variable owned by another scope.

For copyable values the same pattern may be used without `move`:

```text
fn increment(value: int) -> int {
    return value + 1
}

var count = 10
count = increment(count)
```

This rule avoids mutable-alias tracking, mutable-borrow lifetime rules, and hidden cross-scope mutation.

It also makes call sites distinguish read-only use from ownership-transforming use:

```text
inspect(&data)               // read only
data = process(move data)    // transfer, transform, replace
```

The guiding rule is:

> A function may read borrowed state, or own and transform a value; it does not mutate another scope's ordinary binding through a mutable reference.


## 38. Loop syntax

Fast compile uses `for` as its loop construct.

There is no separate `while` keyword in v0.1.

### Counted / conditional loop

The ordinary counted form keeps initialization, condition, and step together.

```text
for var i = 0; i < 100; i = i + 1 {
    process(i)
}
```

The condition expression must have type `bool`.

```text
for var i = 0; i < 100; i = i + 1 {
    ...
}
// valid because i < 100 is bool
```

```text
for var i = 0; i; i = i + 1 {
    ...
}
// compile error: int is not bool
```

This form is preferred when the progression of a loop variable should be visible at the loop header.

### Collection loop

Arrays and slices may be traversed with `for ... in ...`.

```text
for value in values {
    process(value)
}
```

v0.1 does not require a general user-defined iterator protocol merely to support this syntax. The initial form is defined for built-in array/slice-style collections.

### Condition-only loop

A condition-only loop uses `for` directly.

```text
for condition {
    ...
}
```

Here too, `condition` must have type `bool`.

### Infinite loop

An infinite loop is written:

```text
for {
    ...
}
```

### No increment/decrement operators

Fast compile v0.1 has no `++` or `--` operators.

Changes are written explicitly:

```text
i = i + 1
i = i - 1
```

This avoids special prefix/postfix mutation operators and keeps state changes visually explicit.

The guiding rule is:

> Use one loop construct, and write loop-variable changes explicitly.


## 39. Operators

Fast compile v0.1 provides only built-in operators with fixed language-defined meanings.

The initial operator set includes:

```text
+  -  *  /  %
== !=
<  <=  >  >=
&& || !
&  |  ^  ~
<< >>
```

### No user-defined operator overloading

Users cannot redefine operators for structs or other user-defined types.

For example:

```text
struct Vec2 {
    x: float
    y: float
}

const a = Vec2 { x: 1.0, y: 2.0 }
const b = Vec2 { x: 3.0, y: 4.0 }

const c = a + b // compile error
```

The compiler does not search for a hidden user-defined meaning of `+` for `Vec2`.


This remains true even for a struct with only one numeric field.

```text
struct Weight {
    value: float
}

const a = Weight { value: 1.0 }
const b = Weight { value: 2.0 }

const c = a + b             // compile error
const d = a.value + b.value // allowed, result is float
```

Fast compile does not implicitly unwrap a struct to its only field or treat single-field structs as aliases for that field's type.

If a project wants vector addition, it uses an ordinary named function:

```text
fn add_vec2(a: Vec2, b: Vec2) -> Vec2 {
    return Vec2 {
        x: a.x + b.x,
        y: a.y + b.y
    }
}

const c = add_vec2(a, b)
```

This keeps operator resolution local and predictable.

### Short-circuit boolean operators

`&&` and `||` use short-circuit evaluation.

```text
if condition_a && condition_b {
    ...
}
```

`condition_b` is evaluated only if `condition_a` is true.

For `||`, the right side is evaluated only if the left side is false.

The guiding rule is:

> Operators have fixed built-in meanings; user-defined behavior uses named functions.


## 40. Functions without return values

A function with no return value may omit the return annotation entirely.

A function with no parameters may use either an empty parameter list or `void`.

```text
fn start() {
    ...
}

fn start(void) {
    ...
}
```

These two forms are semantically equivalent.

Using `void` in the parameter list is only an explicit spelling of "no parameters"; it does not create a parameter and does not make `void` a normal value type.


```text
fn print_user(user: &User) {
    print(user.name)
}
```

The same function may explicitly write `-> void` when that improves readability.

```text
fn print_user(user: &User) -> void {
    print(user.name)
}
```

These two forms are semantically equivalent.

A return-without-value statement is written:

```text
return
```

`void` is not a normal value type in v0.1. Variables, struct fields, arrays, and ordinary values cannot have type `void`.

Its roles are limited to explicitly stating that a function returns no value and explicitly stating that a function takes no parameters.

The guiding rule is:

> No-return functions may stay concise, but `void` may be written when explicitness helps.


## 41. Module-level values

Fast compile allows both `const` and `var` declarations at module/file top level.

```text
const MaxUsers: int = 1000
var activeUsers: int = 0
```

Top-level values belong to their module rather than to a hidden process-wide namespace.

Module visibility is `private` by default; `public` must be written explicitly.

A value may be accessed from another module only when:

1. the declaration is `public`; and
2. the using module explicitly imports the declaring module.

```text
public const MaxUsers: int = 1000
public var activeUsers: int = 0
```

An imported public `const` may be read but not changed.

An imported public `var` may be read and assigned to.

External access should remain visibly associated with the imported module namespace rather than becoming an implicit global name.

Conceptually:

```text
import server_state

print(server_state.activeUsers)
server_state.activeUsers = 10
```

If a module is not imported, its public values are not directly readable or writable from that source module.

This keeps global state explicit in the dependency graph and visible at call sites.

Initialization-order rules for non-trivial module-level values are a separate design decision.

The guiding rule is:

> Global state exists only as explicit module state, and cross-module access requires an explicit dependency.


## 42. Member access and module qualification

Fast compile uses different syntax for value/member access and module/name qualification.

### Value and struct member access

A dot (`.`) accesses a member of a value or struct.

```text
struct User {
    name: string
}

var user = User {
    name: "Taro"
}

print(user.name)
```

### Module qualification

A double colon (`::`) accesses an exported symbol from a module or namespace.

```text
import server_state

print(server_state.state::activeUsers)
server_state.state::activeUsers = 10
```

The distinction is intentional:

```text
value.member        // member of a value
module::symbol      // symbol exported by a module
```

The two forms may be combined when a module-level value is a struct.

```text
graphics.config::settings.resolution
```

Here:

```text
graphics::settings  // module-qualified value
.resolution         // field access on that value
```

This keeps namespace resolution visually separate from data access.

The guiding rule is:

> Use `.` for values and `::` for namespaces/modules.


## 43. Visibility

Top-level declarations belong to their `.fscm` file module.

Fast compile uses two visibility levels:

- `private`: accessible only within the same file module;
- `public`: accessible from other file modules once the containing package is imported where necessary.

If no visibility keyword is written, the declaration is `private` by default.

This applies to top-level functions, structs, enums, constants, variables, and other declarations.

```text
fn helper(value: int) -> int {
    ...
}

public fn run(void) {
    ...
}
```

A private declaration, including any declaration with no visibility keyword, may be used only within the same `.fscm` file.

A public declaration may be referenced from another file module.

From outside the package, the caller must import the package and qualify the reference with package + file module:

```text
import server

server.post::run()
```

Importing the package does not expose private declarations.

```text
server.post::helper(10) // compile error: helper is private
```

Public file-level values follow their mutability rules:

```text
public const MaxUsers: int = 1000
public var activeUsers: int = 0
```

From another package:

```text
print(server.config::MaxUsers)
server.state::activeUsers = 10
```

The guiding rule is:

> A file is encapsulated by default; only explicitly public declarations cross the file boundary.


## 44. Module initialization

Fast compile does not execute function calls at module/file top level.

Top-level declarations may use only initializers that are compile-time evaluable.

```text
var count: int = 0
const name: string = "server"
```

These are allowed.

```text
var config = load_config()
var socket = open_socket()
```

These are compile errors because they require runtime function calls during module initialization.

Importing a module never runs hidden initialization code.

If a module needs runtime setup, it exposes an ordinary function and the caller invokes it explicitly.

```text
// server.fscm

var activeUsers: int = 0

public fn initialize(void) {
    activeUsers = load_initial_user_count()
}
```

```text
// main.fscm

import server

fn main(void) {
    server::initialize()
    ...
}
```

This makes initialization order part of ordinary visible control flow rather than an implicit property of the import graph.

Fast compile has no global constructors and no hidden per-module startup functions generated from top-level expressions.

Whether uninitialized module-level storage is ever permitted is a separate design decision; this rule does not imply that ordinary variables may exist without a valid initial value.

The guiding rule is:

> Imports declare dependencies; functions perform runtime initialization.


## 45. Initialization and struct field defaults

Fast compile does not allow uninitialized variables.

Every variable must have a valid value at the point where it is declared.

```text
var x: int        // compile error
var user: User    // compile error
```

```text
var x: int = 0
```

This rule also applies to module-level values.

### Struct field defaults

To avoid making large structs unnecessarily verbose, a struct field may declare an explicit default value.

```text
struct Config {
    retries: int = 3
    verbose: bool = false
    name: string = ""
}
```

When constructing the struct, fields with declared defaults may be omitted.

```text
var config = Config {
    name: "server"
}
```

This is equivalent to explicitly providing the declared defaults for the omitted fields.

A field without a declared default must always be provided.

```text
struct User {
    id: int
    name: string
    active: bool = true
}

var user = User {
    id: 1,
    name: "Taro"
}
```

The compiler does not invent zero/default values for fields that do not declare one.

### Default-expression restrictions

Struct field defaults must be cheap and compile-time evaluable.

They may not call arbitrary functions or perform runtime initialization.

This preserves the rule that hidden initialization work does not occur.

The guiding rule is:

> Variables are never uninitialized, and omitted struct fields are allowed only when the struct itself explicitly defines their defaults.


## 46. Nested structs and recursive layout

Structs may contain other structs by value.

```text
struct Position {
    x: float
    y: float
}

struct Player {
    id: int
    pos: Position
}
```

Without defaults, nested values must be initialized explicitly.

```text
var player = Player {
    id: 1,
    pos: Position {
        x: 0.0,
        y: 0.0
    }
}
```

Explicit field defaults may be used to reduce nested initialization boilerplate.

```text
struct Position {
    x: float = 0.0
    y: float = 0.0
}

struct Player {
    id: int
    pos: Position = Position {}
}

var player = Player {
    id: 1
}
```

A struct may not contain itself recursively by value, directly or indirectly, because that would make its size infinite or undefined.

```text
struct Node {
    next: Node
}
// compile error
```

Indirect recursive value layouts are also rejected.

```text
struct A {
    b: B
}

struct B {
    a: A
}
// compile error
```

Opaque pointers such as `ptr<T>` have fixed size and therefore do not create an infinite layout, but they remain non-dereferenceable in ordinary Fast compile code.

The compiler determines struct layout from the declared field graph and rejects recursive by-value cycles.

The guiding rule is:

> Nested structs are ordinary values; recursive by-value layouts are not allowed.


## 47. Package import and file-module syntax

Fast compile does not use a `module` declaration keyword.

The filesystem defines names:

```text
server/post.fscm             -> package server, module post
graphics/renderer.fscm      -> package graphics, module renderer
net/http.fscm                -> package net, module http
server/admin/user.fscm      -> package server.admin, module user
```

A dependency on a package is declared with `import`.

```text
import server
import graphics
import net
```

A public symbol is accessed as:

```text
package.file::symbol
```

Examples:

```text
server.post::letter
graphics.renderer::draw
net.http::get
```

This deliberately keeps the source file that defines a symbol visible at the use site.

### Import aliases

A package import may declare a local alias with `as`.

```text
import graphics as gfx

gfx.renderer::draw()
```

The alias changes only the package qualifier.

### Same-package references

Files inside the same package must still import their own package when they access another file module.

```text
// server/user.fscm

import server

server.post::letter()
```

This keeps all cross-file dependencies explicit, even when both files are in the same directory/package.

Private declarations remain private to their own file and are not reachable from sibling files.

### No recursive folder import

Importing a package exposes only that exact directory/package.

```text
import server
```

does not import `server.admin`.

To use a child package:

```text
import server.admin

server.admin.user::create()
```

### No wildcard imports or symbol injection

Fast compile v0.1 does not support wildcard imports.

```text
import server::*
import *
```

are invalid.

Importing a package also never injects file modules or symbols as unqualified local names.

The guiding rule is:

> Import packages for convenience, but keep the defining file explicit at every cross-file symbol use.


## 48. Imports are not transitive

Fast compile package imports are direct dependencies only.

If package `server` imports package `database`, and another package imports `server`, the importer does not automatically gain source-level access to `database`.

```text
// application/main.fscm

import server

server.post::send()       // allowed
database.query::run()     // compile error: database is not imported
```

To use `database` directly, the file/package must import it explicitly.

```text
import server
import database

server.post::send()
database.query::run()
```

Imports are not re-exported in v0.1.

A package also does not recursively expose child packages merely because of directory nesting.

Compiled interface metadata may internally load dependency interfaces needed to describe public signatures, but that does not grant source-level namespace access.

The guiding rule is:

> You may directly use only the packages you imported yourself. Cross-file access always requires an import, even inside the same package.


## 49. Conditional branching

Fast compile uses ordinary `if / else if / else` control flow.

```text
if (score >= 100) {
    print("high")
} else if (score >= 50) {
    print("middle")
} else {
    print("low")
}
```

The condition of an `if` must have type `bool`.

```text
if (true) {
    ...
}
```

is valid.

Fast compile does not implicitly convert integers, pointers, strings, or other values to `bool`.

```text
var x: int = 10

if (x) {
    ...
}
// compile error
```

The condition must be written explicitly.

```text
if (x != 0) {
    ...
}
```

Opaque pointers follow the same rule.

```text
if (pointer != null) {
    ...
}
```

In v0.1, `if` is a control statement rather than a value-producing expression.

The guiding rule is:

> Conditions are always explicit boolean expressions.


## 50. Not decided yet

The following areas are still open:

- Windows object/executable format and linker strategy
- Linker strategy
- Final concrete syntax
- C-compatible struct layout details
- C++ wrapper/adaptor tooling details

These should be decided with the primary goal of keeping compilation fast.
