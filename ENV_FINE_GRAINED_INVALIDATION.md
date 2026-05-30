# Exploration: fine-grained env() invalidation

Status: analysis / recommendation (task #6). No code change proposed yet.

## Current behaviour (whole-tree, "free")

`setEnv`/`removeEnv` bump `Config::version_`. The per-node layout cache gate in
`calculateLayoutInternal` (CalculateLayout.cpp:2300) is:

```cpp
const bool needToVisitNode =
    (node->isDirty() && layout->generationCount != generationCount) ||
    layout->configVersion != node->getConfig()->getVersion() ||
    layout->lastOwnerDirection != ownerDirection;
```

So after any env change, every node's stored `layout->configVersion` mismatches
the new version → `needToVisitNode == true` for the whole tree → full relayout
on the next `YGNodeCalculateLayout`. Cost is O(tree) regardless of how many
nodes actually reference the changed variable.

Two things already soften this and need no further work:

1. **Value-change dedupe (already implemented).** `Config::setEnv` only bumps
   `version_` when the value actually changes (`if (envValues_[id] != value)`).
   Re-setting an unchanged value (e.g. the same safe-area every frame) is a
   no-op — no relayout.
2. **Lazy, not eager.** The bump invalidates caches but does no work until the
   next layout pass; redundant bumps between passes coalesce.

## When whole-tree actually hurts

Only when **all** of these hold: a large tree, frequent env changes, and the
changed variable is referenced by **few** nodes located **away from the root**.
If env drives root-level geometry (root padding = safe area — the canonical
case), the change propagates to most of the tree anyway, so fine-graining saves
little. The win is real only for _sparse, deep_ references.

## Fine-grained design (Option A: per-id → nodes index)

Idea: replace the global `version_` bump with targeted dirtying.

- Add `mutable std::unordered_map<uint16_t, /*nodes*/> envIdToNodes_` on Config.
- Populate it when an expression containing `Kind::Env(id)` is stored on a node
  (the setter has both node and config) and remove on clear.
- On `setEnv(id)`/`removeEnv(id)` (when the value changes): instead of bumping
  `version_`, iterate `envIdToNodes_[id]` and call
  `node->markDirtyAndPropagate()`.

Why this is _correct_: `markDirtyAndPropagate` dirties the node and its ancestor
chain (size changes ripple up). Descendants recompute naturally — when an
ancestor is re-laid-out, children see different available space / percentage
bases, so their measurement-cache lookups miss. So dirtying just the referencing
nodes covers the whole affected region.

### Why it is not free — the blocking costs

1. **Node lifecycle / dangling pointers (the hard part).** Config would hold raw
   `Node*`s. Nodes are freed via `YGNodeFree`/`YGNodeFreeRecursive` with no
   notification to the Config. A freed node left in `envIdToNodes_` → use-after-
   free on the next `setEnv`. Fixing this needs a free-hook (node tells its
   config to de-index) or a generational/weak handle — new lifecycle coupling
   between Node and Config that does not exist today.
2. **Config moves.** `setConfig` already carries a documented cross-config
   caveat for interned ids; a node index compounds it (must de-index from the
   old config, re-index into the new).
3. **Thread-safety.** The index is mutated during set-style and read/mutated
   during `setEnv`; today env state is only `mutable` maps touched off the
   layout path. A node index widens the surface for the "don't mutate shared
   config/nodes concurrently" contract.
4. **Memory + bookkeeping** per referencing node, plus dedupe when one node
   references the same id from multiple properties.

### Option B (rejected): per-node "usesEnv" bit to skip non-env nodes

Gate on `configVersion mismatch && node->usesEnv()`. Unsound on its own: a node
that doesn't reference env can still need recompute because an env-sized
ancestor changed its available space / percentage base. Would still require
propagation from the env nodes, i.e. most of Option A's machinery, for no extra
benefit.

## Recommendation

**Keep whole-tree invalidation as the default.** It is correct, zero-cost in
code, and the two softeners above already cover the common "same value re-set"
path. Pursue Option A only if profiling shows env-driven full-tree relayout is a
_measured_ bottleneck on a real workload (large tree, sparse deep references,
high env-change frequency). If pursued, the gating work item is the
Node↔Config lifecycle hook (item 1), not the dirtying itself.

Cheap, safe follow-ups that do **not** require the index:

- (Done) value-change dedupe in `setEnv`.
- Optionally batch multi-variable updates (e.g. all four safe-area insets in one
  host call) so they share a single version bump / single relayout — minor.
