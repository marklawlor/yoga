/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <yoga/numeric/FloatOptional.h>
#include <cstdint>
#include <vector>

namespace facebook::yoga {

class Config;

/**
 * ExpressionNode represents one node in a CSS math expression tree.
 *
 * Leaf nodes (Value, Percent, Number) hold a float value.
 * Function nodes (Min, Max, Clamp, Add, Subtract, Multiply, Divide) hold
 * indices into the flat ExpressionPool vector pointing to child nodes.
 *
 * All nodes for a single expression are stored in a contiguous flat vector
 * (ExpressionPool). The root node is at a known index within that vector.
 *
 * CSS constraints enforced at evaluation time:
 *   Multiply: at least one operand must resolve to a unitless Number
 *   Divide:   right operand must resolve to a unitless Number
 */
struct ExpressionNode {
  enum class Kind : uint8_t {
    // Leaf nodes
    Value, // concrete unitless value
    Percent, // percentage of referenceLength
    Number, // unitless — used as operand in Multiply/Divide
    Env, // CSS env() — late-bound, resolved from Config at layout time
    // Binary function nodes
    Min,
    Max,
    Add,
    Subtract,
    Multiply,
    Divide,
    // Ternary
    Clamp, // a=min, b=val, c=max
  };

  Kind kind{Kind::Value};

  union {
    float floatValue{0.0f}; // Value, Percent, Number
    struct {
      uint16_t a;
      uint16_t b;
      uint16_t c; // Clamp only
    } children;
  };

  constexpr static ExpressionNode value(float v) {
    ExpressionNode n;
    n.kind = Kind::Value;
    n.floatValue = v;
    return n;
  }
  constexpr static ExpressionNode percent(float v) {
    ExpressionNode n;
    n.kind = Kind::Percent;
    n.floatValue = v;
    return n;
  }
  constexpr static ExpressionNode number(float v) {
    ExpressionNode n;
    n.kind = Kind::Number;
    n.floatValue = v;
    return n;
  }

  // Sentinel value for the unused .c field in binary nodes.
  // Using 0xFFFF instead of 0 means an accidental .c read on a binary node
  // produces an out-of-bounds assert rather than silently re-evaluating [0].
  static constexpr uint16_t kUnusedChild = 0xFFFF;

  constexpr static ExpressionNode min(uint16_t a, uint16_t b) {
    ExpressionNode n;
    n.kind = Kind::Min;
    n.children = {a, b, kUnusedChild};
    return n;
  }
  constexpr static ExpressionNode max(uint16_t a, uint16_t b) {
    ExpressionNode n;
    n.kind = Kind::Max;
    n.children = {a, b, kUnusedChild};
    return n;
  }
  // env(name) / env(name, fallback).
  // a = interned name id, b = fallback root index (or kUnusedChild if none),
  // c = kUnusedChild (unused).
  constexpr static ExpressionNode env(
      uint16_t nameId,
      uint16_t fallbackIdx = kUnusedChild) {
    ExpressionNode n;
    n.kind = Kind::Env;
    n.children = {nameId, fallbackIdx, kUnusedChild};
    return n;
  }
  constexpr static ExpressionNode
  clamp(uint16_t minIdx, uint16_t valIdx, uint16_t maxIdx) {
    ExpressionNode n;
    n.kind = Kind::Clamp;
    n.children = {minIdx, valIdx, maxIdx};
    return n;
  }
  constexpr static ExpressionNode add(uint16_t a, uint16_t b) {
    ExpressionNode n;
    n.kind = Kind::Add;
    n.children = {a, b, kUnusedChild};
    return n;
  }
  constexpr static ExpressionNode subtract(uint16_t a, uint16_t b) {
    ExpressionNode n;
    n.kind = Kind::Subtract;
    n.children = {a, b, kUnusedChild};
    return n;
  }
  constexpr static ExpressionNode multiply(uint16_t a, uint16_t b) {
    ExpressionNode n;
    n.kind = Kind::Multiply;
    n.children = {a, b, kUnusedChild};
    return n;
  }
  constexpr static ExpressionNode divide(uint16_t a, uint16_t b) {
    ExpressionNode n;
    n.kind = Kind::Divide;
    n.children = {a, b, kUnusedChild};
    return n;
  }

  // Required by std::vector::operator== used in Style equality comparison.
  constexpr bool operator==(const ExpressionNode& other) const {
    if (kind != other.kind) {
      return false;
    }
    switch (kind) {
      case Kind::Value:
      case Kind::Percent:
      case Kind::Number:
        return floatValue == other.floatValue;
      case Kind::Env:
        // Compare name id (a) and fallback index (b). c is always kUnusedChild.
        // The name id is per-Config; this comparison is only meaningful within
        // the same Config (the sole caller, storeExpression's redundant-set
        // check, always compares within one node's pool). It must remain
        // non-crashing for cross-config Style equality, which it is.
        return children.a == other.children.a &&
            children.b == other.children.b;
      default: // all function nodes use children
        return children.a == other.children.a &&
            children.b == other.children.b && children.c == other.children.c;
    }
  }
  constexpr bool operator!=(const ExpressionNode& other) const {
    return !(*this == other);
  }
};

/**
 * Evaluate an expression tree rooted at pool[rootIndex] against
 * referenceLength (the parent's dimension used to resolve %).
 *
 * Returns the concrete unitless value wrapped in FloatOptional.
 * Returns FloatOptional{} (undefined) if the expression is IACVT.
 */
FloatOptional evaluate(
    const std::vector<ExpressionNode>& pool,
    uint16_t rootIndex,
    float referenceLength,
    const Config* config = nullptr);

} // namespace facebook::yoga
