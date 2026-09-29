# 3. データ型・関数・制御構文

## struct

class・継承・virtual method・暗黙constructorは持たない。structは値データ + 静的method。

```fscm
struct User {
    id: int;
    name: string;
    active: bool = true;
}
```

fieldに明示defaultがあれば生成時に省略できる。defaultがないfieldは必須。

structはstructをby-valueで持てるが、直接・間接の再帰by-value layoutは禁止。

method receiverは暗黙の `this`。methodは常に読み取り専用。

```fscm
fn User.print(void) -> void {
    print(this.name);
    this.debug();
}
```

`user.print();` は `user` を変更・消費しない。変更したい処理は通常関数 + `move` + 呼び出し元再代入を使う。

## enum

enumは明示値を持つ `int` 定数集合。自動採番・payload・pattern matchingなし。

```fscm
enum State {
    Idle = 0,
    Running = 1
}
```

## Result

例外は持たない。回復可能な失敗は組み込み `Result<T,E>`。

状態は `Ok(T)` / `Err(E)` の2つ。

- `.ok` -> bool
- `.value` -> T
- `.error` -> E

誤った側をruntimeで読んだ場合はprogram errorとして終了。`try/catch/throw`、unwind、`?` はv0.1にない。

## generics

user-defined genericsは許可するが、normal buildで型ごとに本文を再parse/type-check/codegenしない。

`T` はopaque typeとして扱い、move・borrow・格納・return等は可能だが、任意 `T` に `+` や任意methodがあるとは仮定できない。

trait / concept / specialization / template metaprogrammingはv0.1にない。

必要な size/alignment/move/drop/layout は小さいtype descriptorで渡す。

## 関数

戻り値なしは以下を同義とする。

```fscm
fn work(void) { }
fn work(void) -> void { }
```

引数なしは `()` と `(void)` を同義とする。

`void` は通常の値型ではなく、no-parameter / no-returnの明示専用。

同一 `.fscm` ファイル内で関数名は一意。別ファイルなら同名可。関数overloadは行わない。

## main

有効なentry pointは次の2つのみ。

```fscm
fn main(void) { }
fn main(void) -> int { return 0; }
```

前者は正常終了でexit code 0。command-line argumentsは標準library/platform APIから明示取得する。

## 文・空白

simple statement/declarationは `;` で終了。改行・indentationは文法上の意味を持たない。

blockは `{}`。function/if/for/while/switchの `}` 後に `;` は不要。

## if

`if (condition)` のconditionは必ず `bool`。int、pointer、string等からboolへの暗黙変換なし。ifは値を返すexpressionではない。

## for / while

`for` は3節形式。

```fscm
for (initialization; condition; step) { }
```

- initializationは最大1文。
- stepは最大1文。
- comma operatorなし。
- conditionが存在する場合は必ずbool。
- 各節は省略可能。condition省略はtrue。
- `for (;;)` は無限loop。
- `for ... in` はない。

`while (condition)` はcondition 1つだけで必ずbool。`while (true)` は無限loop。

`++` / `--` はない。`i = i + 1;` と明示する。

`break;` / `continue;` は最内側loopへ作用。label付きはv0.1にない。

## switch

switch対象は整数型、enum、string。

- fallthroughなし。
- case末尾のbreak不要。
- case値はcompile-time evaluable。
- 重複caseはcompile error。
- stringはUTF-8 byte列の完全一致。
- default省略可。

## operator

組み込み:

`+ - * / % == != < <= > >= && || ! & | ^ ~ << >>`

user-defined operator overloadなし。`&&` / `||` はshort-circuit。

`==` / `!=` は具体的な値型を構造比較する。stringはlength + bytes、structはfield順、arrayはlength/要素順、Resultはactive state + active value。最初の不一致で終了。

`< <= > >=` は数値型とstringのみ。stringはUTF-8 byte列の辞書順。enum、bool、struct、array、Result、ptrはorderingなし。
