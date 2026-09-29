# Fast compile 設計仕様

> **この日本語文書群が正本です。**
> 英語版は翻訳であり、差異がある場合は日本語版を優先します。

以前の `DESIGN.md` は3,000行を超えていたため、仕様を章別に分割しました。`DESIGN.md` は目次と正本の入口だけを担当します。

## 章

1. [設計目標・原則](design/01-principles.md)
2. [型・所有権・メモリ](design/02-types-memory.md)
3. [データ型・関数・制御構文](design/03-language-core.md)
4. [package・import・incremental build](design/04-modules-build.md)
5. [ランタイム・FFI・並行処理](design/05-runtime-ffi.md)
6. [コンパイラ・optimizer・backend](design/06-compiler.md)
7. [プラットフォーム・ツールチェーン](design/07-platform.md)
8. [未決定事項](design/08-open-issues.md)

## 翻訳

- [English translation](DESIGN.en.md)

英訳は参考用です。仕様変更は日本語正本へ先に反映します。
