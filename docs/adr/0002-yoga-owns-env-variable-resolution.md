# ADR-0002: Yoga owns env() variable resolution

## Status

Accepted. Supersedes the `env()` clause of [ADR-0001](./0001-yoga-owns-css-math-expression-evaluation.md), which assigned `env()` resolution to the consumer ("`var()` substitution, custom property cascading, and `env()` resolution remain the consumer's responsibility"). `var()` substitution and custom-property cascading remain the consumer's responsibility; only `env()` moves into Yoga.

## Context

ADR-0001 bundled `env()` with `var()`/cascade and pushed all three to the consumer. But `env()` is materially different from `var()`:

- It is a **flat, document/UA-scoped name → value lookup**, not a cascaded custom property. There is no parent-chain walk, no token-sequence storage, no defining-vs-using-node `%` subtlety — the things that made "full CSS in Yoga" undesirable in ADR-0001.
- Its canonical values (`safe-area-inset-*`) are **owned by the host/UA and change at runtime** (device rotation, keyboard). Resolving them in the consumer _before_ the value reaches Yoga forces the consumer to re-push styles on every change.

Late binding — Yoga storing the _name_ and resolving the _value_ each layout pass — is the only model that lets a host update one value and relayout without re-assigning node styles. That requires Yoga to own the table, which `referenceLength`-only evaluation (ADR-0001) cannot provide.

## Decision

`env(name)` / `env(name, fallback)` is a new leaf in the existing expression AST (`ExpressionNode::Kind::Env`), reusing all the ADR-0001 machinery (flat pool, `StyleValueHandle::Expression`, depth bounds, equality, serialise).

- **Scope/owner:** each `Config` owns the `id → value` table (per-surface, the CSS-document analogue). Not per-node. The `name → id` mapping is process-global (see Interning).
- **Late binding:** the node stores an interned `uint16` name id (`children.a`); the fallback is a sub-expression (`children.b`). The value is resolved during layout by threading a `const Config*` into the single `Style::resolve()` funnel and its wrappers (callers already hold the node), reading that config's `id → value`.
- **Interning (revised — see "Interning is process-global" below):** name → id is a **process-global, append-only, thread-safe registry** (`facebook::yoga::internEnvName`, a free function), shared across all Configs and threads; ids are never reused. Each `Config` holds only `id → FloatOptional value`. `YGExpressionSerialise(expr)` needs **no Config** — a serialised id is valid in any Config. The registry is touched only at serialise / `setEnv` time (a brief mutex), never in the layout hot path (eval reads the per-Config value vector by id).
- **Value type:** point-only `float`; `Kind::Env` is length-compatible — never `<number>`, never a legal divisor. So `calc(100% - env(x))` and `calc(env(x) * 2)` are valid, `calc(10 / env(x))` is IACVT.
- **Unset/fallback:** value is `FloatOptional`; unset+fallback → fallback subtree, unset+no-fallback → IACVT/undefined. `YGConfigSetEnv(YGUndefined)` ≡ `YGConfigRemoveEnv` (clears the value, keeps the name).
- **Defaults:** `safe-area-inset-{top,right,bottom,left}` pre-seeded to `0`; hosts may override/remove. All other names are host-defined.
- **API:** `YGConfigSetEnv/RemoveEnv/GetEnv`, `YGExpressionEnv(name, fallback)`, plus the `env(...)` string parser; mirrored in JS and Java bindings.

## Interning is process-global (revised decision)

The first implementation interned `name → id` **per `Config`** (config-aware `YGExpressionSerialise(expr, config)`), to avoid a process global. That was reversed in favour of a **process-global** registry, driven by the React Native (Fabric) integration:

- RN's prop-conversion path serialises expressions with **no `Config`/node in scope** (`YogaStylableProps::setProp`'s signature is fixed by the `Props` base; `convertRawProp` runs before the node exists). Per-Config, serialise-time interning is therefore impossible there.
- RN gives **every shadow node its own `yoga::Config`** (`YogaLayoutableShadowNode::yogaConfig_`), so per-Config ids would also be inconsistent between the config that serialised and the config that resolves.

A process-global `name → id` makes `YGExpressionSerialise(expr)` config-free again (matching the math-expression pattern RN already vendors, so the RN prop layer needs **zero changes**) and makes ids config-independent (resolving the cross-config caveat). The cost — a small, append-only, mutex-guarded global string table — is confined to the cold path (serialise / `setEnv`); the layout hot path stays a pure `config->envValues_[id]` read. The alternative (storing the name string per-Style and doing a string lookup at eval) was rejected because it pushes strings into the layout hot path. Values remain per-Config (`Config::envValues_`, indexed by the global id).

## Invalidation

`setEnv`/`removeEnv` bump `Config::version_` (and only when the value actually changes). The existing `calculateLayoutInternal` cache gate (`layout->configVersion != node->getConfig()->getVersion()`) then forces a relayout of every node under that Config on the next `CalculateLayout`. This reuses existing machinery with zero new code.

**Whole-tree invalidation is the accepted default.** Fine-grained invalidation (dirtying only nodes that reference the changed variable, via a per-id → nodes index + `markDirtyAndPropagate`) was explored and **deferred**: it is correct but its blocking cost is a new Node↔Config lifecycle hook (freed nodes must de-index or risk use-after-free), and its benefit only materialises for sparse, deep references — not the common root-level safe-area case, which propagates widely anyway. Pursue only if profiling shows a measured bottleneck. See `ENV_FINE_GRAINED_INVALIDATION.md`.

## Consequences

- Hosts set `env()` values once on a Config and relayout; no per-change style re-push. Matches how `env()` behaves on the web.
- All consumers (C, JS, Java) get `env()` on every length-accepting property for free, exactly as ADR-0001 delivered math functions.
- Cross-config node moves (`setConfig`) are safe: name ids are process-global, so a stored id resolves correctly under any config (it reads that config's `id → value`, defaulting to undefined if the name is unset there). (Under the original per-Config interning this was a documented caveat; global interning removes it.)
- `env()` values are point-only; a percentage or `calc()` is still expressible as a _fallback_, but a stored env value is always a point length.
- `var()` and custom-property cascade remain out of scope and the consumer's responsibility, consistent with ADR-0001.

## Alternatives rejected

- **Early binding (resolve at set time).** Bake the value into a concrete node when the expression is set. Needs no threading, but "set/remove env later" cannot retroactively update already-set styles — defeating the runtime-update use case.
- **Global/static env table.** Simplest API but breaks multi-surface/multi-thread layout: two surfaces with different safe-areas would collide.
- **Per-node env values.** Wrong CSS semantics, bloats every style, forces redundant sets across nodes referencing the same variable.
