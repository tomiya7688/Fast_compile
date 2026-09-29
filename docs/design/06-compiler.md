# 6. コンパイラ・optimizer・backend

## 配布形態

同じFast compileバージョンで2つの公式compiler variantを提供する。

```text
Fast compile X.Y.Z
├─ 軽量高速版
└─ 独自最適化版
```

両者はsource language、parser、type system、ownership、diagnostics、interface metadata、incremental dependency format、core IRを共有し、optimizer/codegen pathだけが異なる。

## 軽量高速版

normal development build向け。

- Fast compile IRから直接native object生成。
- 安価で予測可能なlocal optimizationのみ。
- whole-program最適化なし。
- function-level parallel codegen向け。
- LLVMなしで完全動作。

## 独自最適化版

Fast compile専用optimizer + native backend。

optimizerは巨大なopaque engineではなく、小さい独立passを積み上げる。

候補pass:

- constant folding / propagation
- dead code / unreachable block elimination
- copy propagation
- simple local CSE
- branch simplification
- strength reduction
- local stack-slot elimination
- cheap function-local range analysis
- 安全証明できたbounds check除去
- 安全証明できたoverflow check除去
- register allocation準備

証明が簡単かつ確実でない場合、安全checkは残す。

各pass前後にIR verifierを実行可能にし、invalid IRならmachine codeを生成しない。

軽量版と独自最適化版は同じ入力で同じ観測可能動作をしなければならず、differential testに利用する。

## LLVM

LLVMは任意の第3backend。高コスト最適化、実験、validation、比較用に残す。公式2variantの必須dependencyにはしない。

## 実装言語

初期compiler実装はC。

parser、byte処理、hash table、file/object format、OS APIなどsystems workloadとの相性を優先する。

handwritten assemblyはprofilingで実際にbottleneckと確認した局所routineだけに使う。

基本は高品質なC compilerのoptimizerを信頼し、assembly版には可能な限りportable C fallback/referenceを残す。
