/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// C++-only helper (returns std::vector, includes C++ headers). Yoga's podspec
// sweeps yoga/*.h into the public, C-importable umbrella header, so this body
// must be hidden from C / Objective-C translation units — otherwise importing
// the `yoga` clang module from Obj-C (e.g. via RCTConvert) fails to parse the
// C++ and the whole module build breaks.
#ifdef __cplusplus

#include <yoga/style/StyleExpression.h>
#include <vector>

struct YGExpression;

/**
 * Serialise a YGExpression builder tree into a flat ExpressionNode vector
 * with the root at index 0, for passing to StyleValuePool::store().
 *
 * env() variable names are interned to stable ids by the process-global
 * registry (facebook::yoga::internEnvName), so no Config is needed here — a
 * serialised id is valid in any Config. This keeps serialisation callable from
 * contexts with no node/config in scope (e.g. React Native's Fabric prop path).
 * Implemented in YGExpression.cpp.
 */
std::vector<facebook::yoga::ExpressionNode> YGExpressionSerialise(
    const YGExpression* expr);

#endif // __cplusplus
