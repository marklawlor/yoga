/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <yoga/YGMacros.h>

YG_EXTERN_C_BEGIN

/**
 * Opaque handle to a CSS math expression builder tree.
 * Build an expression using the YGExpression* constructors, set it on a node
 * with YGNodeStyleSet*Expression(), then free the builder with
 * YGExpressionFree(). The expression is deep-copied into the node's style
 * pool on set, so the builder may be freed immediately after setting.
 */
typedef struct YGExpression* YGExpressionRef;

YG_EXPORT YGExpressionRef YGExpressionValue(float value);
YG_EXPORT YGExpressionRef YGExpressionPercent(float value);
YG_EXPORT YGExpressionRef YGExpressionNumber(float value);
YG_EXPORT YGExpressionRef YGExpressionMin(YGExpressionRef a, YGExpressionRef b);
YG_EXPORT YGExpressionRef YGExpressionMax(YGExpressionRef a, YGExpressionRef b);
YG_EXPORT YGExpressionRef YGExpressionClamp(
    YGExpressionRef min,
    YGExpressionRef val,
    YGExpressionRef max);
YG_EXPORT YGExpressionRef YGExpressionAdd(YGExpressionRef a, YGExpressionRef b);
YG_EXPORT YGExpressionRef
YGExpressionSubtract(YGExpressionRef a, YGExpressionRef b);
YG_EXPORT YGExpressionRef
YGExpressionMultiply(YGExpressionRef a, YGExpressionRef b);
YG_EXPORT YGExpressionRef
YGExpressionDivide(YGExpressionRef a, YGExpressionRef b);

/**
 * Free an expression builder that has NOT been set on a node.
 * Do not call on a NULL pointer. The builder may be freed immediately after
 * setting on a node — the node holds its own copy.
 */
YG_EXPORT void YGExpressionFree(YGExpressionRef expr);

/**
 * Parse a Yoga math expression string into an expression builder.
 *
 * Supported syntax: unitless numbers (32, 16.5), percentages (50%),
 * min(a,b), max(a,b), clamp(min,val,max), calc(expr), and the four
 * arithmetic operators inside calc(). Whitespace is ignored.
 *
 * Returns NULL if parsing fails, including: CSS unit suffixes (px, em),
 * var(), env(), unknown functions, or invalid syntax.
 *
 * The caller owns the returned expression and must call YGExpressionFree()
 * after setting it on a node.
 */
YG_EXPORT YGExpressionRef YGExpressionParse(const char* expression);

YG_EXTERN_C_END
