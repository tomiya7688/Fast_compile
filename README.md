# Fast compile

**Fast compile** は、大規模なネイティブプロジェクトを非常に短い時間でビルドすることを最優先に設計するプログラミング言語です。

> 目標は「C++より速い」だけではありません。成熟版では、代表的な clean build と incremental build の両方で Go より速いことを合格条件とします。

## 5つの優先事項

1. **コンパイラ自身を壊れにくくする。** miscompile、クラッシュ、不正なキャッシュ再利用を極力避ける。
2. **言語仕様起因のバグを減らす。** 自動解放、単純な所有権、範囲チェック、overflowチェックなどを持つ。
3. **コンパイルを非常に速くする。** 不要な解析・探索・再コンパイル・展開を避ける。
4. **生成プログラムを軽く速くする。** ネイティブコード、小さいランタイム、GCなし。
5. **コンパイラ本体も必要以上に巨大化させない。**

要約すると、

> **信頼できるコンパイラ。バグを生みにくい言語。高速ビルド。軽量なネイティブ実行。小さいコンパイラ。**

## 現在の主な設計

- GCなし。所有値はスコープ終了時に自動解放。
- 所有権移動は `move` で明示。
- 借用 `&T` は読み取り専用で、関数境界を越えて保存・returnできない。
- 可変借用なし。変更したい所有値は `move` で渡し、結果を呼び出し元で再代入する。
- 暗黙数値変換、関数overload、operator overload、macro、任意のcompile-time実行なし。
- `.fscm` ファイル単位のコンパイル、symbol/function単位のincremental cache。
- packageはフォルダ、file moduleは `.fscm` ファイル。
- 軽量高速コンパイラと独自最適化コンパイラを同一バージョンで提供。
- LLVMは任意の第3backend。
- 初期ターゲットは Linux x86-64。LinuxではELF `.o` を生成し、既定リンカは mold。

## 仕様

日本語版が正本です。

- [設計仕様（日本語・正本）](Docs/Jp/README.md)
- [English documentation](Docs/en/README.md)

## Bitlangとの関係

Fast compile は Bitlang と同じ作者による別系統の言語で、Bitlangファミリーではありません。

Bitlang はプリプロセッサやコンパイラを巨大で柔軟な変換系へ拡張する方向を取ります。Fast compile は逆に、互換変換・高度な解析・動的な探索・隠れたcompile-time処理を極力削り、コンパイル量そのものを減らす方向を取ります。

両者は同じ問題に対する軽量版/重量版ではなく、意図的に逆のトレードオフを選ぶ別言語です。

## Cコンパイラ実装

C製コンパイラは Linux x86-64 向けのELF `.o` 出力を実装します。現在の実装は空モジュールと、仕様で定義した次の `main` を受け付けます。

```fscm
fn main(void) { }
fn main(void) -> int { return 0; }
```

未対応構文は成功扱いにせず、位置を示す診断を出して終了します。現段階で構文全体や最適化を実装済みとは扱いません。

### ビルドとテスト

開発者はCMakeとC11対応Cコンパイラを使い、コンパイラと自動テストをビルドします。

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

CTestは開発時の検証用です。`fastcompile` のCLIにはテスト実行や最終リンク機能を含めません。

### コンパイル

利用者はLinux x86-64上で `.fscm` ファイルを指定し、ELF relocatable objectを生成します。

```sh
build/fastcompile source.fscm -o output.o
```

## 状態

仕様を正本として初期コンパイラ実装を進めています。言語全体、ABI、最適化、incremental buildは未実装です。

[English documentation](Docs/en/README.md)
