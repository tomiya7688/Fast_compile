# 2. 型・所有権・メモリ

## GCと所有権

GCは持たない。heap resourceには原則1つの所有者があり、所有者がスコープを抜けると自動解放する。

```fscm
fn work(void) {
    const data = make_data();
} // data が所有するheap resourceを自動解放
```

早期 `return;` でも同じ。明示 `free(value);` を行った場合、その値は以後使用不可とする。

所有権移動は `move` で明示する。

```fscm
const b = move a;
```

move後の `a` は所有値として使用できない。

## 借用

`&T` は読み取り専用の一時借用。

- 関数へ渡せる。
- 所有権を移さない。
- returnできない。
- global、struct、container、closureなど長寿命な場所へ保存できない。
- 関数を越えるlifetime解析は行わない。

可変借用 `&mut T` / `&var T` は存在しない。

値を変更する関数は所有権を受け取り、変更後の値を返す。

```fscm
data = process(move data);
```

## Copyとmove-only

以下はCopy型:

`sbyte`, `byte`, `short`, `ushort`, `int`, `uint`, `long`, `ulong`, `float`, `double`, `bool`, enum, `ptr<T>`。

所有resourceを含む型はmove-only。struct、固定長配列、`Result<T,E>` などのCopy可否は構成要素から再帰的に決める。

## 数値型

| 型 | 意味 |
|---|---|
| `sbyte` | signed 8-bit |
| `byte` | unsigned 8-bit |
| `short` | signed 16-bit |
| `ushort` | unsigned 16-bit |
| `int` | signed 32-bit |
| `uint` | unsigned 32-bit |
| `long` | signed 64-bit |
| `ulong` | unsigned 64-bit |
| `float` | 32-bit floating point |
| `double` | 64-bit floating point |
| `bool` | `true` / `false` |

整数リテラルの既定型は `int`、小数リテラルは `float`。

暗黙の数値型変換は行わない。宣言型が明示されたリテラルは最初からその型として解釈する。

通常の整数演算はoverflow checkを行う。compile-timeに分かればcompile error、runtimeで発生すればプログラム終了。silent wraparoundはしない。

## 型推論

型推論は宣言時の右辺だけを見る。後続文、呼び出し元、別関数、return先から逆算しない。

```fscm
const x = 10;      // int
var y = get_id();  // get_idの戻り値型
```

関数引数・戻り値型は使用箇所から推論しない。

## const / var

`const` は再代入不可、`var` は再代入可。どちらも型は宣言時に固定。

`const` はcompile-time constant専用ではない。runtime式で初期化してもよい。

## string

- `char` 型なし。
- `string` はUTF-8。
- `string[index]` は `byte`。
- `string.length` はUTF-8 byte数。
- 内容はimmutable。
- `string` はmove-only。

64-bit targetでの表現は16byte固定:

```text
data pointer       : 64 bit
tagged byte length : 64 bit
```

`tagged byte length` の最上位1bitをheap所有bit、残り63bitをbyte長とする。

- literalはstatic dataを指し、ownership bit = 0。drop時freeしない。
- 動的生成stringはownership bit = 1。drop時 `fc_free(pointer)`。
- capacityは持たない。

## 配列・slice

- `[T; N]`: 固定長所有配列。`N` は型の一部。
- `array<T>`: 動的所有配列。概念上 pointer + length + capacity。
- `&[T]`: 読み取り専用借用slice。概念上 pointer + length。

通常の配列/slice indexはbounds checkあり。compile-timeに明らかな範囲外はcompile error、runtime値は実行時check。

unchecked indexingと一般 `unsafe` modeはv0.1に存在しない。

## null / ptr

通常値には一般的なnullable型を設けない。

```text
&T          non-null
&[T]        non-null
string      non-null
array<T>    non-null
ptr<T>      null可・dereference不可
```

`ptr<T>` はFFI等のopaque handle。保存・比較・引数・戻り値には使えるが、`*p` や `p.field` は存在しない。
