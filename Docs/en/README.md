# Fast compile documentation — English translation

> **Translation notice:** The normative specification is the Japanese documentation under [`Docs/Jp/`](../Jp/README.md).
> If the English translation differs from the Japanese specification, the Japanese specification takes precedence.

Fast compile is a programming language designed primarily to make very large native projects compile extremely quickly.

## Core priorities

1. Keep the compiler reliable.
2. Reduce bugs caused by the language itself.
3. Make compilation extremely fast.
4. Keep generated native programs reasonably small and fast.
5. Keep the compiler implementation itself from becoming unnecessarily large.

A mature implementation is expected to beat representative Go builds in both clean-build and incremental-build performance.

## Specification

- [English design specification](design-specification.md)
- [Japanese normative specification](../Jp/README.md)

The English documents are translations and may lag behind the Japanese specification.
