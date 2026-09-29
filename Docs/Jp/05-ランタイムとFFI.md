# 5. ランタイム・FFI・並行処理

## 小さいランタイム

通常プログラムに以下を必須としない。

- GC
- exception unwind
- reflection metadata
- VM
- 大きなobject runtime
- background thread
- async executor
- hidden allocation registry

標準libraryとlanguage runtimeは分離し、未使用機能をリンクしない。

## allocator

language/runtimeが定義するallocation境界は概念上次の3つ。

```text
fc_alloc(size)
fc_realloc(pointer, size)
fc_free(pointer)
```

初期Linux実装はsystem `malloc/realloc/free` を使うが、言語仕様として固定しない。

custom allocator、fixed pool、embedded allocator、将来のno-heap profileへ差し替え可能にする。

`array<T>` 等の各instanceにallocator pointerを持たせない。

## C ABI / FFI

v0.1はC ABIを正式対応し、C++ ABIを直接実装しない。

`ptr<T>` はopaque pointer。null可、dereference不可。

FFIで直接渡せる中心型は固定幅scalarと `ptr<T>`。`string`、`array<T>`、`Result<T,E>`、通常generic struct等は暗黙変換しない。

C++ libraryはC互換wrapper経由で利用し、将来Fast compile側にwrapper/adaptor toolを用意する。

## closure

closureは許可するがcaptureは明示。

- implicit captureなし。
- scalar Copy値は値capture。
- owning valueは `move` capture必須。
- borrowed referenceをcaptureして保存できない。
- hidden heap allocationなし。
- closure環境は匿名struct相当。
- callbackは概念上 invoke function pointer + environment pointer のborrowed callable。
- non-capturing closureは普通のfunction pointer相当へ落とせる。

## thread / async

v0.1はOS native threadあり、async/awaitなし。

thread spawnへ渡すclosureは必要データを所有し、borrowed referenceをthreadへ持ち出さない。

`thread.spawn` はsmall handleを返し、`join` は明示。handle dropでrunning threadを停止させない。

mutex、condition variable、atomic等は標準library/platform layerの薄いwrapperまたはintrinsicとする。

## macro / compile-time execution

v0.1にgeneral macro systemと任意のuser compile-time code実行はない。

localで小さく決定的なconstant expressionのみcompiler評価可能。大量code generationが必要なら外部generatorで通常 `.fscm` sourceを生成し、build graphへ明示する。
