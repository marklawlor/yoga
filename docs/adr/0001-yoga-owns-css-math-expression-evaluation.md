# ADR-0001: Yoga owns CSS math expression evaluation

## Status

Accepted

## Context

Yoga needs to support CSS math functions (`min()`, `max()`, `clamp()`, `calc()`) on layout properties. These functions can contain `%` operands, and `%` cannot be resolved without the `referenceLength` — the parent's computed pixel dimension — which is only known mid-layout inside Yoga's `calculateLayout` pass.

Three implementation models were considered.

## Decision

Yoga implements an internal **Expression AST and evaluator**. Consumers (e.g. React Native) construct expression trees using Yoga's API, set them on node style properties, and Yoga evaluates them internally during layout. No application-level callback is required.

`var()` substitution, custom property cascading, and `env()` resolution remain the consumer's responsibility and are performed before an expression reaches Yoga. By the time Yoga receives an expression it is variable-free, but may contain `%` operands.

## Consequences

- Every Yoga consumer (JavaScript, Java, Swift bindings) gets CSS math functions on all `StyleLength` properties for free.
- Expressions are owned per-property per-node, copied into the node's `StyleValuePool` on set — consistent with how all existing style values are stored.
- `StyleValueHandle` gains a new `Expression` type, pointing to an expression tree stored in `StyleValuePool`.
- The Expression Evaluator is a pure function of `(ExpressionNode, referenceLength)` with no platform dependencies.

## Known limitations — note for PR description

Expression operands are restricted to concrete value types: `Points`, `Percent`, and `Number` (unitless). `StyleSizeLength` keywords (`fit-content`, `max-content`, `stretch`) are **not supported as expression operands** in this implementation.

Valid: `min(200px, 50%)`, `calc(100% - 32px)`, `clamp(16px, 5%, 48px)`
Invalid: `min(fit-content, 200px)` — keyword operand not yet supported

Keyword operands require evaluating intrinsic content size mid-expression, which is a full sub-layout rather than a simple arithmetic operation. This is left as future work and should be called out explicitly in the PR description.

## Alternatives rejected

### Callback model

Yoga fires a per-property callback during layout, passing `referenceLength`. The consumer evaluates the expression and returns a concrete value. Rejected because it requires every consumer to implement expression evaluation, diverges from the inline HTML model (where the engine evaluates, not the application), and adds a function-pointer-per-node overhead.

### Full CSS in Yoga (including `var()`, `env()`, cascade)

Yoga would own the entire CSS value pipeline. Rejected because: custom properties are stored as token sequences (not computed values) and are resolved by the CSS engine layer (RN's shadow tree), not the layout engine; `%` in a variable value is resolved at the using node, not the defining node, so cascade and `%` resolution are independent concerns; and adding cascade to Yoga would require parent-chain walks inside the layout algorithm with no benefit to non-RN consumers.

### Pre-resolution by consumer (no Yoga expression support)

Consumer resolves all expressions before passing concrete values to Yoga. Rejected because `%` operands cannot be resolved without `referenceLength`, which only Yoga knows during layout.
