/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <cassert>

#include <yoga/config/Config.h>
#include <yoga/debug/Log.h>
#include <yoga/numeric/Comparison.h>
#include <yoga/style/StyleExpression.h>

namespace facebook::yoga {

static FloatOptional finiteOrUndefined(float v) {
  return (yoga::isinf(v) || yoga::isUndefined(v)) ? FloatOptional{} : FloatOptional{v};
}

// Returns true if the subtree at idx resolves to a unitless <number> type.
//
// Multiply and Divide are intentionally NOT propagated as <number>: in Yoga's
// unitless model their result is a concrete scalar that is length/percentage
// compatible (e.g. `2 * 16` is the length 32, not a dimensionless number), so
// it must combine additively with lengths and percentages. Treating them as
// <number> would wrongly reject natural expressions like `calc(100% - 2 * 16)`
// as IACVT. A bare Number leaf is still a <number>, so the genuine
// `<number> + <length>` mismatch is still caught.
static bool isNumberType(
    const std::vector<ExpressionNode>& pool,
    uint16_t idx) {
  const auto& node = pool[idx];
  switch (node.kind) {
    case ExpressionNode::Kind::Number:
      return true;
    case ExpressionNode::Kind::Add:
    case ExpressionNode::Kind::Subtract:
    case ExpressionNode::Kind::Min:
    case ExpressionNode::Kind::Max:
      return isNumberType(pool, node.children.a) &&
          isNumberType(pool, node.children.b);
    case ExpressionNode::Kind::Clamp:
      return isNumberType(pool, node.children.a) &&
          isNumberType(pool, node.children.b) &&
          isNumberType(pool, node.children.c);
    case ExpressionNode::Kind::Env:
      // env() is length-compatible (point value), never a <number>.
      return false;
    default: // Value, Percent, Multiply, Divide
      return false;
  }
}

// Returns true if the subtree contains NO percentage leaves — i.e., it is safe
// to use as a CSS divisor. Yoga's parser rejects unit suffixes (px, em, …), so
// Value nodes from string parsing are always bare scalars, not lengths.
static bool isDivisorType(
    const std::vector<ExpressionNode>& pool,
    uint16_t idx) {
  const auto& node = pool[idx];
  switch (node.kind) {
    case ExpressionNode::Kind::Number:
    case ExpressionNode::Kind::Value:
      return true;
    case ExpressionNode::Kind::Percent:
      return false;
    case ExpressionNode::Kind::Env:
      // env() is a length value, never a legal divisor.
      return false;
    case ExpressionNode::Kind::Add:
    case ExpressionNode::Kind::Subtract:
    case ExpressionNode::Kind::Multiply:
    case ExpressionNode::Kind::Divide:
    case ExpressionNode::Kind::Min:
    case ExpressionNode::Kind::Max:
      return isDivisorType(pool, node.children.a) &&
          isDivisorType(pool, node.children.b);
    case ExpressionNode::Kind::Clamp:
      return isDivisorType(pool, node.children.a) &&
          isDivisorType(pool, node.children.b) &&
          isDivisorType(pool, node.children.c);
    default:
      return false;
  }
}

FloatOptional evaluate(
    const std::vector<ExpressionNode>& pool,
    uint16_t idx,
    float ref,
    const Config* config) {
  assert(idx < pool.size());
  const ExpressionNode& node = pool[idx];
  switch (node.kind) {
    case ExpressionNode::Kind::Value:
      return finiteOrUndefined(node.floatValue);
    case ExpressionNode::Kind::Percent:
      return finiteOrUndefined(node.floatValue * ref * 0.01f);
    case ExpressionNode::Kind::Number:
      return finiteOrUndefined(node.floatValue);
    case ExpressionNode::Kind::Env: {
      // Late-bound: look up the value by interned name id from the Config's
      // env store. If defined, use it (fallback ignored). Else if a fallback
      // subtree exists, evaluate it. Else IACVT -> undefined.
      if (config != nullptr) {
        FloatOptional stored = config->getEnvValueById(node.children.a);
        if (stored.isDefined()) {
          return finiteOrUndefined(stored.unwrap());
        }
      }
      if (node.children.b != ExpressionNode::kUnusedChild) {
        return evaluate(pool, node.children.b, ref, config);
      }
      return FloatOptional{};
    }
    case ExpressionNode::Kind::Min: {
      // CSS spec: all arguments must be the same type — mixing a <number> with
      // a <length>/percentage among the args is IACVT (same rule as Add/Sub).
      const bool leftIsNumber = isNumberType(pool, node.children.a);
      const bool rightIsNumber = isNumberType(pool, node.children.b);
      if (leftIsNumber != rightIsNumber) {
        yoga::log(
            LogLevel::Warn,
            "yoga-expression: invalid calc() — min() arguments must be the "
            "same type (mixing <number> with <length> is IACVT)\n");
        return FloatOptional{};
      }
      auto a = evaluate(pool, node.children.a, ref, config);
      auto b = evaluate(pool, node.children.b, ref, config);
      if (!a.isDefined() || !b.isDefined())
        return FloatOptional{};
      return finiteOrUndefined(std::min(a.unwrap(), b.unwrap()));
    }
    case ExpressionNode::Kind::Max: {
      // CSS spec: same type requirement as min().
      const bool leftIsNumber = isNumberType(pool, node.children.a);
      const bool rightIsNumber = isNumberType(pool, node.children.b);
      if (leftIsNumber != rightIsNumber) {
        yoga::log(
            LogLevel::Warn,
            "yoga-expression: invalid calc() — max() arguments must be the "
            "same type (mixing <number> with <length> is IACVT)\n");
        return FloatOptional{};
      }
      auto a = evaluate(pool, node.children.a, ref, config);
      auto b = evaluate(pool, node.children.b, ref, config);
      if (!a.isDefined() || !b.isDefined())
        return FloatOptional{};
      return finiteOrUndefined(std::max(a.unwrap(), b.unwrap()));
    }
    case ExpressionNode::Kind::Clamp: {
      // CSS spec: all three arguments must be the same type — mixing a
      // <number> with a <length>/percentage among min/val/max is IACVT.
      const bool minIsNumber = isNumberType(pool, node.children.a);
      const bool valIsNumber = isNumberType(pool, node.children.b);
      const bool maxIsNumber = isNumberType(pool, node.children.c);
      if (minIsNumber != valIsNumber || valIsNumber != maxIsNumber) {
        yoga::log(
            LogLevel::Warn,
            "yoga-expression: invalid calc() — clamp() arguments must be the "
            "same type (mixing <number> with <length> is IACVT)\n");
        return FloatOptional{};
      }
      auto minVal = evaluate(pool, node.children.a, ref, config);
      auto val = evaluate(pool, node.children.b, ref, config);
      auto maxVal = evaluate(pool, node.children.c, ref, config);
      if (!minVal.isDefined() || !val.isDefined() || !maxVal.isDefined())
        return FloatOptional{};
      return finiteOrUndefined(
          std::max(minVal.unwrap(), std::min(val.unwrap(), maxVal.unwrap())));
    }
    case ExpressionNode::Kind::Add: {
      // CSS spec: both operands must be the same "type" — either both <number>
      // or both dimension/percentage. <length> + <number> is IACVT.
      const bool leftIsNumber = isNumberType(pool, node.children.a);
      const bool rightIsNumber = isNumberType(pool, node.children.b);
      if (leftIsNumber != rightIsNumber) {
        yoga::log(
            LogLevel::Warn,
            "yoga-expression: invalid calc() — Add operands must be the same "
            "type (<number> + <length> is IACVT)\n");
        return FloatOptional{};
      }
      auto a = evaluate(pool, node.children.a, ref, config);
      auto b = evaluate(pool, node.children.b, ref, config);
      if (!a.isDefined() || !b.isDefined())
        return FloatOptional{};
      return finiteOrUndefined(a.unwrap() + b.unwrap());
    }
    case ExpressionNode::Kind::Subtract: {
      // CSS spec: same type requirement as Add.
      const bool leftIsNumber = isNumberType(pool, node.children.a);
      const bool rightIsNumber = isNumberType(pool, node.children.b);
      if (leftIsNumber != rightIsNumber) {
        yoga::log(
            LogLevel::Warn,
            "yoga-expression: invalid calc() — Subtract operands must be the "
            "same type (<number> - <length> is IACVT)\n");
        return FloatOptional{};
      }
      auto a = evaluate(pool, node.children.a, ref, config);
      auto b = evaluate(pool, node.children.b, ref, config);
      if (!a.isDefined() || !b.isDefined())
        return FloatOptional{};
      return finiteOrUndefined(a.unwrap() - b.unwrap());
    }
    case ExpressionNode::Kind::Multiply: {
      // CSS spec: in `a * b`, at least one operand must be a <number>. Anything
      // else — length*length, percent*length, percent*percent — is IACVT. A
      // multiply/divide subtree is NOT a <number> (see isNumberType), so e.g.
      // `(50% + 0) * (50% + 0)` (two length-typed operands) correctly resolves
      // to undefined rather than squaring the resolved lengths.
      if (!isNumberType(pool, node.children.a) &&
          !isNumberType(pool, node.children.b)) {
        yoga::log(
            LogLevel::Warn,
            "yoga-expression: invalid calc() — Multiply requires at least one "
            "<number> operand (IACVT)\n");
        return FloatOptional{};
      }
      auto a = evaluate(pool, node.children.a, ref, config);
      auto b = evaluate(pool, node.children.b, ref, config);
      if (!a.isDefined() || !b.isDefined())
        return FloatOptional{};
      return finiteOrUndefined(a.unwrap() * b.unwrap());
    }
    case ExpressionNode::Kind::Divide: {
      // CSS spec: right operand must be dimensionless — no Percent leaves.
      // Yoga's parser rejects unit suffixes, so Value nodes are bare scalars.
      if (!isDivisorType(pool, node.children.b)) {
        yoga::log(
            LogLevel::Warn,
            "yoga-expression: invalid calc() — Divide right operand must be "
            "dimensionless (IACVT)\n");
        return FloatOptional{};
      }
      auto b = evaluate(pool, node.children.b, ref, config);
      if (!b.isDefined() || b.unwrap() == 0.0f) {
        yoga::log(
            LogLevel::Warn,
            "yoga-expression: invalid calc() — division by zero (IACVT)\n");
        return FloatOptional{};
      }
      auto a = evaluate(pool, node.children.a, ref, config);
      if (!a.isDefined())
        return FloatOptional{};
      return finiteOrUndefined(a.unwrap() / b.unwrap());
    }
  }
  return FloatOptional{};
}

} // namespace facebook::yoga
