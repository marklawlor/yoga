/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <yoga/YGExpression.h>
#include <yoga/YGExpressionInternal.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include <yoga/style/StyleExpression.h>

using namespace facebook::yoga;

struct YGExpression {
  ExpressionNode node;
  std::unique_ptr<YGExpression> left{};
  std::unique_ptr<YGExpression> right{};
  std::unique_ptr<YGExpression>
      maxChild{}; // Clamp only — the max (third) operand

  static std::unique_ptr<YGExpression> leaf(ExpressionNode n) {
    auto e = std::make_unique<YGExpression>();
    e->node = n;
    return e;
  }
};

static uint16_t flatten(
    const YGExpression* expr,
    std::vector<ExpressionNode>& out) {
  uint16_t idx = static_cast<uint16_t>(out.size());
  out.push_back(expr->node);

  if (expr->left) {
    assert(expr->right != nullptr);
    uint16_t leftIdx = flatten(expr->left.get(), out);
    uint16_t rightIdx = flatten(expr->right.get(), out);
    assert(
        expr->node.kind != ExpressionNode::Kind::Clamp ||
        expr->maxChild != nullptr);
    out[idx].children.a = leftIdx;
    out[idx].children.b = rightIdx;
    if (expr->maxChild) {
      uint16_t maxIdx = flatten(expr->maxChild.get(), out);
      out[idx].children.c = maxIdx;
    }
    // binary nodes: .c retains kUnusedChild sentinel from factory init
  }
  return idx;
}

static std::vector<ExpressionNode> serialise(const YGExpression* expr) {
  std::vector<ExpressionNode> nodes;
  flatten(expr, nodes);
  return nodes;
}

YGExpressionRef YGExpressionValue(float value) {
  return YGExpression::leaf(ExpressionNode::value(value)).release();
}
YGExpressionRef YGExpressionPercent(float value) {
  return YGExpression::leaf(ExpressionNode::percent(value)).release();
}
YGExpressionRef YGExpressionNumber(float value) {
  return YGExpression::leaf(ExpressionNode::number(value)).release();
}

static YGExpressionRef makeBinary(
    ExpressionNode (*factory)(uint16_t, uint16_t),
    YGExpressionRef a,
    YGExpressionRef b) {
  // A NULL child would leave left/right unset, so flatten() would treat this as
  // a leaf and the binary node's .a/.b would keep their factory-init index of
  // 0 — a self-reference that causes unbounded recursion at evaluate() time.
  assert(a != nullptr && b != nullptr);
  auto e = std::make_unique<YGExpression>();
  e->node = factory(0, 0);
  e->left.reset(a);
  e->right.reset(b);
  return e.release();
}

YGExpressionRef YGExpressionMin(YGExpressionRef a, YGExpressionRef b) {
  return makeBinary(ExpressionNode::min, a, b);
}
YGExpressionRef YGExpressionMax(YGExpressionRef a, YGExpressionRef b) {
  return makeBinary(ExpressionNode::max, a, b);
}
YGExpressionRef YGExpressionAdd(YGExpressionRef a, YGExpressionRef b) {
  return makeBinary(ExpressionNode::add, a, b);
}
YGExpressionRef YGExpressionSubtract(YGExpressionRef a, YGExpressionRef b) {
  return makeBinary(ExpressionNode::subtract, a, b);
}
YGExpressionRef YGExpressionMultiply(YGExpressionRef a, YGExpressionRef b) {
  return makeBinary(ExpressionNode::multiply, a, b);
}
YGExpressionRef YGExpressionDivide(YGExpressionRef a, YGExpressionRef b) {
  return makeBinary(ExpressionNode::divide, a, b);
}

YGExpressionRef YGExpressionClamp(
    YGExpressionRef min,
    YGExpressionRef val,
    YGExpressionRef max) {
  assert(min != nullptr && val != nullptr && max != nullptr);
  auto e = std::make_unique<YGExpression>();
  e->node = ExpressionNode::clamp(0, 0, ExpressionNode::kUnusedChild);
  e->left.reset(min);
  e->right.reset(val);
  e->maxChild.reset(max);
  return e.release();
}

void YGExpressionFree(YGExpressionRef expr) {
  delete expr;
}

std::vector<ExpressionNode> YGExpressionSerialise(const YGExpression* expr) {
  return serialise(expr);
}

// --- String parser ---

namespace {

// Bound on nesting depth. Every nested sub-expression (calc/min/max/clamp/
// parenthesised group) passes through parsePrimary exactly once, so capping the
// depth there bounds parser, flatten(), and evaluate() recursion and prevents a
// stack overflow on hostile input like `calc(calc(calc(...)))`.
static constexpr int kMaxExpressionDepth = 128;

struct Parser {
  const char* pos;
  const char* end;
  int depth = 0;

  // RAII guard that increments the nesting depth for the lifetime of a
  // parsePrimary call and reports whether the limit was exceeded.
  struct DepthGuard {
    int& depth;
    bool ok;
    explicit DepthGuard(int& d) : depth(d), ok(++d <= kMaxExpressionDepth) {}
    ~DepthGuard() {
      --depth;
    }
  };

  void skipWhitespace() {
    while (pos < end &&
           (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')) {
      ++pos;
    }
  }

  // Parse a number literal (integer or decimal, optional leading minus).
  // Returns true and sets `out` on success.
  bool parseNumber(float& out) {
    skipWhitespace();
    const char* start = pos;
    if (pos < end && *pos == '-')
      ++pos;
    // Accept either a digit or a leading-dot decimal (.5 is valid CSS)
    if (pos >= end || (*pos != '.' && (*pos < '0' || *pos > '9'))) {
      pos = start;
      return false;
    }
    while (pos < end && *pos >= '0' && *pos <= '9')
      ++pos;
    if (pos < end && *pos == '.') {
      ++pos;
      while (pos < end && *pos >= '0' && *pos <= '9')
        ++pos;
    }
    // Reject CSS unit suffixes (px, em, rem, pt, vw, vh, etc.)
    // % is handled as a separate token, not here
    if (pos < end &&
        ((*pos >= 'a' && *pos <= 'z') || (*pos >= 'A' && *pos <= 'Z'))) {
      pos = start;
      return false;
    }
    // Parse the float via strtof on a null-terminated stack buffer.
    // std::from_chars for float is not available on all platforms (Android NDK).
    char buf[64];
    size_t len = static_cast<size_t>(pos - start);
    if (len >= sizeof(buf)) {
      pos = start;
      return false;
    }
    std::memcpy(buf, start, len);
    buf[len] = '\0';
    char* endPtr;
    out = std::strtof(buf, &endPtr);
    // Require the whole lexeme to be consumed. strtof honours the C locale's
    // LC_NUMERIC decimal separator, so under a comma-decimal locale "12.5"
    // would otherwise parse as 12 and silently drop ".5". Rejecting a partial
    // parse turns that into a clean parse failure instead of a wrong value.
    if (endPtr != buf + len) {
      pos = start;
      return false;
    }
    return true;
  }

  // Consume a specific character, skipping leading whitespace.
  bool consume(char c) {
    skipWhitespace();
    if (pos < end && *pos == c) {
      ++pos;
      return true;
    }
    return false;
  }

  // Match a keyword (case-sensitive) at current position (after whitespace).
  bool matchKeyword(const char* kw) {
    skipWhitespace();
    size_t len = strlen(kw);
    if (static_cast<size_t>(end - pos) < len)
      return false;
    if (strncmp(pos, kw, len) != 0)
      return false;
    // Must be followed by '(' or whitespace (including newlines) or end
    if (pos + len < end && pos[len] != '(' && pos[len] != ' ' &&
        pos[len] != '\t' && pos[len] != '\n' && pos[len] != '\r')
      return false;
    return true;
  }

  // Parse a primary: number, percentage, or function call.
  // When asNumber is true, bare numerals are emitted as Number (for * and /).
  YGExpressionRef parsePrimary(bool asNumber = false) {
    DepthGuard guard(depth);
    if (!guard.ok)
      return nullptr;
    skipWhitespace();
    if (pos >= end)
      return nullptr;

    // calc(expr) — strip the calc wrapper, parse inner
    if (matchKeyword("calc")) {
      pos += 4;
      if (!consume('('))
        return nullptr;
      YGExpressionRef inner = parseExpr();
      if (!inner || !consume(')')) {
        YGExpressionFree(inner);
        return nullptr;
      }
      return inner;
    }
    // min(a, b)
    if (matchKeyword("min")) {
      pos += 3;
      if (!consume('('))
        return nullptr;
      YGExpressionRef a = parseExpr();
      if (!a || !consume(',')) {
        YGExpressionFree(a);
        return nullptr;
      }
      YGExpressionRef b = parseExpr();
      if (!b || !consume(')')) {
        YGExpressionFree(a);
        YGExpressionFree(b);
        return nullptr;
      }
      return YGExpressionMin(a, b);
    }
    // max(a, b)
    if (matchKeyword("max")) {
      pos += 3;
      if (!consume('('))
        return nullptr;
      YGExpressionRef a = parseExpr();
      if (!a || !consume(',')) {
        YGExpressionFree(a);
        return nullptr;
      }
      YGExpressionRef b = parseExpr();
      if (!b || !consume(')')) {
        YGExpressionFree(a);
        YGExpressionFree(b);
        return nullptr;
      }
      return YGExpressionMax(a, b);
    }
    // clamp(min, val, max)
    if (matchKeyword("clamp")) {
      pos += 5;
      if (!consume('('))
        return nullptr;
      YGExpressionRef mn = parseExpr();
      if (!mn || !consume(',')) {
        YGExpressionFree(mn);
        return nullptr;
      }
      YGExpressionRef val = parseExpr();
      if (!val || !consume(',')) {
        YGExpressionFree(mn);
        YGExpressionFree(val);
        return nullptr;
      }
      YGExpressionRef mx = parseExpr();
      if (!mx || !consume(')')) {
        YGExpressionFree(mn);
        YGExpressionFree(val);
        YGExpressionFree(mx);
        return nullptr;
      }
      return YGExpressionClamp(mn, val, mx);
    }
    // Reject any other identifier (var, env, rgb, unknown functions, CSS units)
    if (pos < end &&
        ((*pos >= 'a' && *pos <= 'z') || (*pos >= 'A' && *pos <= 'Z'))) {
      return nullptr;
    }

    // Parenthesised sub-expression
    if (pos < end && *pos == '(') {
      ++pos;
      YGExpressionRef inner = parseExpr();
      if (!inner || !consume(')')) {
        YGExpressionFree(inner);
        return nullptr;
      }
      return inner;
    }

    // Number or percentage
    float num;
    if (!parseNumber(num))
      return nullptr;
    skipWhitespace();
    if (pos < end && *pos == '%') {
      ++pos;
      return YGExpressionPercent(num);
    }
    return asNumber ? YGExpressionNumber(num) : YGExpressionValue(num);
  }

  // Emit bare numerals as Number (unitless) rather than Value — used for * and /.
  YGExpressionRef parsePrimaryAsNumber() {
    return parsePrimary(/*asNumber=*/true);
  }

  // Parse multiplicative: primary (* primary | / primary)*
  YGExpressionRef parseMul() {
    YGExpressionRef lhs = parsePrimary();
    if (!lhs)
      return nullptr;
    while (true) {
      skipWhitespace();
      if (pos < end && *pos == '*') {
        ++pos;
        // Per CSS spec, in 'a * b', at least one operand must be <number>.
        // If the LHS is a bare Value leaf (no unit), upgrade it to Number so
        // that patterns like "2 * 50%" pass the Multiply type check.
        if (!lhs->left && lhs->node.kind == ExpressionNode::Kind::Value) {
          lhs->node.kind = ExpressionNode::Kind::Number;
        }
        // RHS of * must be a Number per CSS spec — use number-context parser
        YGExpressionRef rhs = parsePrimaryAsNumber();
        if (!rhs) {
          YGExpressionFree(lhs);
          return nullptr;
        }
        lhs = YGExpressionMultiply(lhs, rhs);
      } else if (pos < end && *pos == '/') {
        ++pos;
        // Divisor is always a Number per CSS spec
        YGExpressionRef rhs = parsePrimaryAsNumber();
        if (!rhs) {
          YGExpressionFree(lhs);
          return nullptr;
        }
        lhs = YGExpressionDivide(lhs, rhs);
      } else {
        break;
      }
    }
    return lhs;
  }

  // Parse additive: mul (+ mul | - mul)*
  YGExpressionRef parseExpr() {
    YGExpressionRef lhs = parseMul();
    if (!lhs)
      return nullptr;
    while (true) {
      skipWhitespace();
      if (pos < end && *pos == '+') {
        ++pos;
        YGExpressionRef rhs = parseMul();
        if (!rhs) {
          YGExpressionFree(lhs);
          return nullptr;
        }
        lhs = YGExpressionAdd(lhs, rhs);
      } else if (pos < end && *pos == '-') {
        // Be careful: unary minus on a number (e.g. "+ -8") should be handled
        // by parseNumber(), not here. Only treat '-' as subtract if preceded
        // by a complete expression (which we are, since lhs is non-null).
        ++pos;
        YGExpressionRef rhs = parseMul();
        if (!rhs) {
          YGExpressionFree(lhs);
          return nullptr;
        }
        lhs = YGExpressionSubtract(lhs, rhs);
      } else {
        break;
      }
    }
    return lhs;
  }
};

} // anonymous namespace

YGExpressionRef YGExpressionParse(const char* expression) {
  if (expression == nullptr)
    return nullptr;
  size_t len = strlen(expression);
  // Reject empty or implausibly large input. Each node consumes at least one
  // character, so this also keeps the flattened node count well below the
  // uint16_t index ceiling used in StyleExpression.
  constexpr size_t kMaxExpressionLength = 16384;
  if (len == 0 || len > kMaxExpressionLength)
    return nullptr;

  Parser p;
  p.pos = expression;
  p.end = expression + len;

  YGExpressionRef result = p.parseExpr();
  if (!result)
    return nullptr;

  // Reject if there is trailing non-whitespace (incomplete parse)
  p.skipWhitespace();
  if (p.pos != p.end) {
    YGExpressionFree(result);
    return nullptr;
  }
  return result;
}
