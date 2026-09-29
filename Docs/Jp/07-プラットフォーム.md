# 7. プラットフォーム・ツールチェーン

## source extension

Fast compile source fileは `.fscm`。

## target順

1. Linux x86-64
2. Windows x86-64
3. ARM64等は後から

## Linux x86-64

初期軽量backendはELF object file `.o` を生成し、最終linkは外部高速linkerへ渡す。

優先順位:

1. `mold`
2. `ld.lld`
3. system linker

初期段階では独自linkerを作らない。実測でlinkが大きなbottleneckになった場合に、将来Fast compile native incremental linkerを検討する。

## C ABI

Fast compile `long` は常に64bitであり、platformによって幅が変わるCの同名型と名前だけで対応付けない。FFI mappingは実際のwidth/calling conventionに基づく。
