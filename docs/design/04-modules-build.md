# 4. package・import・incremental build

## package / file module

フォルダがpackage、`.fscm` ファイルがfile moduleかつ独立compile unit。

```text
server/
├─ post.fscm
├─ user.fscm
└─ config.fscm
```

参照名は `package.file::symbol`。

```fscm
import server;
server.post::letter();
```

nested directoryは別package。`import server;` は `server.admin` をimportしない。

## import

cross-file accessは同じpackage内でもimport必須。

```fscm
// server/user.fscm
import server;
server.post::letter();
```

importは推移しない。AがBをimportしていても、CがAをimportしただけではBをsource上で使えない。

wildcard importなし。importしたsymbolをlocal namespaceへ暗黙注入しない。

aliasは許可。

```fscm
import graphics as gfx;
gfx.renderer::draw();
```

## visibility

無指定はprivate。`private` は同一 `.fscm` ファイルのみ。`public` は明示し、packageをimportした外部から参照可能。

`public var` は外部から代入可能、`public const` は読み取りのみ。

## `.` と `::`

- `value.member` : 値/struct member。
- `package.file::symbol` : package/file module namespace。

例: `graphics.config::settings.resolution`。

## top-level

top-levelに `const` / `var` / function / struct / enum等を置ける。

top-levelでruntime function callは禁止。importもruntime処理を起こさない。

runtime初期化が必要なら `public fn initialize(...)` 等を明示的に呼ぶ。

未初期化変数は禁止。module-levelも有効な初期値必須。

## compiled interface metadata

他file/packageは依存元source本文を再parseせず、compiler生成の小さいinterface metadataを読む。

interfaceには必要なpublic name、function signature、type/layout、enum値、必要なcompile-time constant等を含める。通常function bodyは含めない。

## incremental dependency

依存追跡はpackage単位だけでなく public symbol単位。

関数bodyだけ変更しpublic interface fingerprintが同じならcallerを再compileしない。

変更file内でもfunction bodyごとにcache keyを持ち、未変更functionはtype-check/codegen結果を再利用できる。

cache keyには少なくともfunction body、signature、実際に使うexternal interface fingerprint、compiler option、target/backend versionを含める。

独立file/functionは並列compile可能。

package/module dependency cycleは許可しない。通常buildでwhole-program type check、cross-module lifetime analysis、LTOを correctness の必須条件にしない。
