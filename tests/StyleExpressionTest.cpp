/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <yoga/YGExpression.h>
#include <yoga/YGExpressionInternal.h>
#include <yoga/style/StyleExpression.h>
#include <limits>

using namespace facebook::yoga;

TEST(StyleExpression, points_leaf) {
  std::vector<ExpressionNode> pool = {ExpressionNode::value(32.0f)};
  EXPECT_FLOAT_EQ(32.0f, evaluate(pool, 0, 1000.0f).unwrap());
}

TEST(StyleExpression, percent_leaf) {
  std::vector<ExpressionNode> pool = {ExpressionNode::percent(50.0f)};
  EXPECT_FLOAT_EQ(100.0f, evaluate(pool, 0, 200.0f).unwrap());
}

TEST(StyleExpression, number_leaf) {
  std::vector<ExpressionNode> pool = {ExpressionNode::number(2.0f)};
  EXPECT_FLOAT_EQ(2.0f, evaluate(pool, 0, 0.0f).unwrap());
}

TEST(StyleExpression, min_two_points) {
  std::vector<ExpressionNode> pool = {
      ExpressionNode::min(1, 2),
      ExpressionNode::value(50.0f),
      ExpressionNode::value(200.0f),
  };
  EXPECT_FLOAT_EQ(50.0f, evaluate(pool, 0, 1000.0f).unwrap());
}

TEST(StyleExpression, min_percent_wins) {
  std::vector<ExpressionNode> pool = {
      ExpressionNode::min(1, 2),
      ExpressionNode::percent(50.0f),
      ExpressionNode::value(200.0f),
  };
  EXPECT_FLOAT_EQ(150.0f, evaluate(pool, 0, 300.0f).unwrap());
}

TEST(StyleExpression, min_point_wins) {
  std::vector<ExpressionNode> pool = {
      ExpressionNode::min(1, 2),
      ExpressionNode::percent(50.0f),
      ExpressionNode::value(200.0f),
  };
  EXPECT_FLOAT_EQ(200.0f, evaluate(pool, 0, 500.0f).unwrap());
}

TEST(StyleExpression, max_two_points) {
  std::vector<ExpressionNode> pool = {
      ExpressionNode::max(1, 2),
      ExpressionNode::value(50.0f),
      ExpressionNode::value(200.0f),
  };
  EXPECT_FLOAT_EQ(200.0f, evaluate(pool, 0, 1000.0f).unwrap());
}

TEST(StyleExpression, clamp) {
  // clamp(16px, 5%, 48px) with ref=200 → clamp(16, 10, 48) → 16
  std::vector<ExpressionNode> pool = {
      ExpressionNode::clamp(1, 2, 3),
      ExpressionNode::value(16.0f),
      ExpressionNode::percent(5.0f),
      ExpressionNode::value(48.0f),
  };
  EXPECT_FLOAT_EQ(16.0f, evaluate(pool, 0, 200.0f).unwrap());
}

TEST(StyleExpression, clamp_val_in_range) {
  // clamp(16px, 50%, 200px) with ref=100 → clamp(16, 50, 200) → 50
  std::vector<ExpressionNode> pool = {
      ExpressionNode::clamp(1, 2, 3),
      ExpressionNode::value(16.0f),
      ExpressionNode::percent(50.0f),
      ExpressionNode::value(200.0f),
  };
  EXPECT_FLOAT_EQ(50.0f, evaluate(pool, 0, 100.0f).unwrap());
}

TEST(StyleExpression, clamp_above_max) {
  // clamp(16px, 50%, 48px) with ref=200 → clamp(16, 100, 48) → 48
  std::vector<ExpressionNode> pool = {
      ExpressionNode::clamp(1, 2, 3),
      ExpressionNode::value(16.0f),
      ExpressionNode::percent(50.0f),
      ExpressionNode::value(48.0f),
  };
  EXPECT_FLOAT_EQ(48.0f, evaluate(pool, 0, 200.0f).unwrap());
}

TEST(StyleExpression, multiply_min_of_numbers_is_number_typed) {
  // Builder-only case: a hand-built min() whose leaves are <number> leaves is
  // <number>-typed, so min(2,3) * 50% resolves (2 * 50 = 100).
  //
  // NOTE: the STRING parser CANNOT produce this. min/max/clamp arguments are
  // parsed via parseExpr() with asNumber=false, so bare numerals inside the
  // function become Value (length) leaves, not Number leaves. Only the
  // immediate bare-numeral operands of '*' and '/' are upgraded to <number>.
  // The real string `calc(min(2, 3) * 50%)` is therefore a length*percentage,
  // which is IACVT — see the multiply_string_min_by_percent_is_iacvt test
  // below for the actual string-path behaviour.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::multiply(1, 4), // [0] multiply(min(2,3), 50%)
      ExpressionNode::min(2, 3), // [1] min(2, 3) → number type
      ExpressionNode::number(2.0f), // [2]
      ExpressionNode::number(3.0f), // [3]
      ExpressionNode::percent(50.0f), // [4]
  };
  auto result = evaluate(pool, 0, 100.0f);
  EXPECT_TRUE(result.isDefined());
  EXPECT_FLOAT_EQ(100.0f, result.unwrap());
}

TEST(StyleExpression, multiply_string_min_by_percent_is_iacvt) {
  // String-path counterpart to the builder test above. The parser emits
  // min(2, 3) with Value (length) leaves, so the min subtree is NOT
  // <number>-typed. With neither operand of '*' a <number>, length*percentage
  // is IACVT per the §3 decision, so calc(min(2, 3) * 50%) resolves to
  // undefined — it does NOT yield 100. The expression parses successfully
  // (ASSERT_NE below): this is invalid at computed-value time, not a parse
  // error.
  YGExpressionRef expr = YGExpressionParse("calc(min(2, 3) * 50%)");
  ASSERT_NE(nullptr, expr);
  std::vector<ExpressionNode> pool = YGExpressionSerialise(expr);
  YGExpressionFree(expr);
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, clamp_min_exceeds_max) {
  // CSS spec: when MIN > MAX, MIN wins.
  // clamp(100px, 50%, 20px) with ref=300 → MIN=100, VAL=150, MAX=20 → result =
  // 100
  std::vector<ExpressionNode> pool = {
      ExpressionNode::clamp(1, 2, 3),
      ExpressionNode::value(100.0f),
      ExpressionNode::percent(50.0f),
      ExpressionNode::value(20.0f),
  };
  EXPECT_FLOAT_EQ(100.0f, evaluate(pool, 0, 300.0f).unwrap());
}

TEST(StyleExpression, add) {
  // calc(100% + 16px) with ref=200 → 216
  std::vector<ExpressionNode> pool = {
      ExpressionNode::add(1, 2),
      ExpressionNode::percent(100.0f),
      ExpressionNode::value(16.0f),
  };
  EXPECT_FLOAT_EQ(216.0f, evaluate(pool, 0, 200.0f).unwrap());
}

TEST(StyleExpression, subtract) {
  // calc(100% - 32px) with ref=375 → 343
  std::vector<ExpressionNode> pool = {
      ExpressionNode::subtract(1, 2),
      ExpressionNode::percent(100.0f),
      ExpressionNode::value(32.0f),
  };
  EXPECT_FLOAT_EQ(343.0f, evaluate(pool, 0, 375.0f).unwrap());
}

TEST(StyleExpression, multiply_by_number) {
  // calc(50% * 2) with ref=100 → 100
  std::vector<ExpressionNode> pool = {
      ExpressionNode::multiply(1, 2),
      ExpressionNode::percent(50.0f),
      ExpressionNode::number(2.0f),
  };
  EXPECT_FLOAT_EQ(100.0f, evaluate(pool, 0, 100.0f).unwrap());
}

TEST(StyleExpression, divide_by_number) {
  // calc(100% / 4) with ref=200 → 50
  std::vector<ExpressionNode> pool = {
      ExpressionNode::divide(1, 2),
      ExpressionNode::percent(100.0f),
      ExpressionNode::number(4.0f),
  };
  EXPECT_FLOAT_EQ(50.0f, evaluate(pool, 0, 200.0f).unwrap());
}

TEST(StyleExpression, nested_calc_of_min) {
  // calc(min(50%, 200px) - 16px) with ref=300
  // min(150, 200) = 150; 150 - 16 = 134
  std::vector<ExpressionNode> pool = {
      ExpressionNode::subtract(1, 4), // [0] root: subtract
      ExpressionNode::min(2, 3), // [1] min
      ExpressionNode::percent(50.0f), // [2]
      ExpressionNode::value(200.0f), // [3]
      ExpressionNode::value(16.0f), // [4]
  };
  EXPECT_FLOAT_EQ(134.0f, evaluate(pool, 0, 300.0f).unwrap());
}

// IACVT (Invalid At Computed-Value Time) tests

TEST(StyleExpression, invalid_multiply_value_by_percent) {
  // calc(2px * 50%) — <length> * <percentage>: neither operand is a <number>,
  // so per §2 ("at least one operand must be a <number>") this is IACVT. A
  // bare Value leaf is a length, not a dimensionless scalar.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::multiply(1, 2),
      ExpressionNode::value(2.0f),
      ExpressionNode::percent(50.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, invalid_multiply_compound_value_by_percent) {
  // calc((2px+3px) * 50%) — the left operand is a length (Add of two Values),
  // the right a percentage; neither is a <number> → IACVT → undefined.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::multiply(1, 4),
      ExpressionNode::add(2, 3),
      ExpressionNode::value(2.0f),
      ExpressionNode::value(3.0f),
      ExpressionNode::percent(50.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, invalid_multiply_two_percents) {
  // calc(50% * 50%) — both operands have units: IACVT → undefined
  std::vector<ExpressionNode> pool = {
      ExpressionNode::multiply(1, 2),
      ExpressionNode::percent(50.0f),
      ExpressionNode::percent(50.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, invalid_divide_by_percent) {
  // calc(100% / 50%) — divisor has units: IACVT → undefined
  std::vector<ExpressionNode> pool = {
      ExpressionNode::divide(1, 2),
      ExpressionNode::percent(100.0f),
      ExpressionNode::percent(50.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, divide_by_zero) {
  // calc(100% / 0) — zero divisor: IACVT → undefined
  std::vector<ExpressionNode> pool = {
      ExpressionNode::divide(1, 2),
      ExpressionNode::percent(100.0f),
      ExpressionNode::number(0.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, invalid_add_length_plus_number) {
  // calc(50px + 2) — <length> + <number>: IACVT → undefined
  std::vector<ExpressionNode> pool = {
      ExpressionNode::add(1, 2),
      ExpressionNode::value(50.0f),
      ExpressionNode::number(2.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, invalid_subtract_number_minus_length) {
  // calc(2 - 50px) — <number> - <length>: IACVT → undefined
  std::vector<ExpressionNode> pool = {
      ExpressionNode::subtract(1, 2),
      ExpressionNode::number(2.0f),
      ExpressionNode::value(50.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, valid_add_length_plus_percent) {
  // calc(50px + 50%) with ref=100 → 50 + 50 = 100
  std::vector<ExpressionNode> pool = {
      ExpressionNode::add(1, 2),
      ExpressionNode::value(50.0f),
      ExpressionNode::percent(50.0f),
  };
  auto result = evaluate(pool, 0, 100.0f);
  EXPECT_TRUE(result.isDefined());
  EXPECT_FLOAT_EQ(100.0f, result.unwrap());
}

TEST(StyleExpression, valid_add_number_plus_number) {
  // calc(2 + 3) — <number> + <number> is valid, result is 5.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::add(1, 2),
      ExpressionNode::number(2.0f),
      ExpressionNode::number(3.0f),
  };
  auto result = evaluate(pool, 0, 0.0f);
  EXPECT_TRUE(result.isDefined());
  EXPECT_FLOAT_EQ(5.0f, result.unwrap());
}

TEST(StyleExpression, divide_by_compound_value_expression) {
  // calc(50% / (2+3)) with ref=100 → 50/5 = 10
  // (2+3) is Add(Value,Value): isDivisorType accepts it, isNumberType would not.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::divide(1, 2),
      ExpressionNode::percent(50.0f),
      ExpressionNode::add(3, 4),
      ExpressionNode::value(2.0f),
      ExpressionNode::value(3.0f),
  };
  auto result = evaluate(pool, 0, 100.0f);
  EXPECT_TRUE(result.isDefined());
  EXPECT_FLOAT_EQ(10.0f, result.unwrap());
}

TEST(StyleExpression, invalid_divide_by_percent_compound) {
  // calc(50% / (2 + 50%)) — divisor contains a percent leaf → IACVT
  std::vector<ExpressionNode> pool = {
      ExpressionNode::divide(1, 2),
      ExpressionNode::percent(50.0f),
      ExpressionNode::add(3, 4),
      ExpressionNode::value(2.0f),
      ExpressionNode::percent(50.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, nan_result_is_undefined) {
  // finiteOrUndefined must reject NaN (inf - inf = NaN).
  // 3.4e38 * 300 * 0.01 overflows float to +inf; +inf - +inf = NaN → undefined.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::subtract(1, 2),
      ExpressionNode::percent(3.4e38f),
      ExpressionNode::percent(3.4e38f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 300.0f).isDefined());
}

// A Multiply/Divide result is a length-compatible value in Yoga's unitless
// model, so it must combine additively with percentages and lengths rather
// than being rejected as <number> ± <length> (IACVT).

TEST(StyleExpression, percent_minus_multiply_of_numbers) {
  // calc(100% - 2 * 16) with ref=375 → 375 - 32 = 343
  std::vector<ExpressionNode> pool = {
      ExpressionNode::subtract(1, 2), // [0]
      ExpressionNode::percent(100.0f), // [1]
      ExpressionNode::multiply(3, 4), // [2]
      ExpressionNode::number(2.0f), // [3]
      ExpressionNode::number(16.0f), // [4]
  };
  auto result = evaluate(pool, 0, 375.0f);
  EXPECT_TRUE(result.isDefined());
  EXPECT_FLOAT_EQ(343.0f, result.unwrap());
}

TEST(StyleExpression, multiply_of_numbers_plus_value) {
  // calc(2 * 3 + 5) → 6 + 5 = 11 (result usable as a length)
  std::vector<ExpressionNode> pool = {
      ExpressionNode::add(1, 4), // [0]
      ExpressionNode::multiply(2, 3), // [1]
      ExpressionNode::number(2.0f), // [2]
      ExpressionNode::number(3.0f), // [3]
      ExpressionNode::value(5.0f), // [4]
  };
  auto result = evaluate(pool, 0, 0.0f);
  EXPECT_TRUE(result.isDefined());
  EXPECT_FLOAT_EQ(11.0f, result.unwrap());
}

// A bare Number leaf is still a <number>, so the genuine mismatch is still
// rejected (regression guard for the isNumberType change above).

TEST(StyleExpression, number_leaf_plus_length_still_invalid) {
  // calc(2 + 50px) — <number> + <length>: IACVT → undefined
  std::vector<ExpressionNode> pool = {
      ExpressionNode::add(1, 2),
      ExpressionNode::number(2.0f),
      ExpressionNode::value(50.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

// Infinity must be normalised to undefined for leaves and min/max/clamp, not
// just for the binary arithmetic operators.

TEST(StyleExpression, value_infinity_is_undefined) {
  std::vector<ExpressionNode> pool = {
      ExpressionNode::value(std::numeric_limits<float>::infinity())};
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, percent_overflow_leaf_is_undefined) {
  // 3.4e38 * 300 * 0.01 overflows float to +inf → undefined
  std::vector<ExpressionNode> pool = {ExpressionNode::percent(3.4e38f)};
  EXPECT_FALSE(evaluate(pool, 0, 300.0f).isDefined());
}

TEST(StyleExpression, max_of_infinity_is_undefined) {
  std::vector<ExpressionNode> pool = {
      ExpressionNode::max(1, 2),
      ExpressionNode::value(std::numeric_limits<float>::infinity()),
      ExpressionNode::value(10.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

// §2: Multiply requires at least one <number> operand. The following exercise
// the stricter rule — length*length and length*percent (where each side is a
// length-typed Add subtree, not a bare <number>) are all IACVT.

TEST(StyleExpression, invalid_multiply_length_expr_by_length_expr) {
  // calc((50% + 0) * (50% + 0)) — each operand is Add(Percent, Value), a
  // length type, not a <number>. Neither side is a <number> → IACVT.
  // (Previously this incorrectly evaluated to 50*50 = 2500.)
  std::vector<ExpressionNode> pool = {
      ExpressionNode::multiply(1, 4), // [0] multiply(add(1), add(4))
      ExpressionNode::add(2, 3), // [1] 50% + 0
      ExpressionNode::percent(50.0f), // [2]
      ExpressionNode::value(0.0f), // [3]
      ExpressionNode::add(5, 6), // [4] 50% + 0
      ExpressionNode::percent(50.0f), // [5]
      ExpressionNode::value(0.0f), // [6]
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, invalid_multiply_length_by_length) {
  // calc(2px * 3px) — two bare lengths (Value leaves), neither a <number> →
  // IACVT → undefined.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::multiply(1, 2),
      ExpressionNode::value(2.0f),
      ExpressionNode::value(3.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, valid_multiply_percent_by_number) {
  // calc(50% * 2) with ref=100 → 100. The RHS is a <number>, so this stays
  // valid under the stricter Multiply rule.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::multiply(1, 2),
      ExpressionNode::percent(50.0f),
      ExpressionNode::number(2.0f),
  };
  auto result = evaluate(pool, 0, 100.0f);
  EXPECT_TRUE(result.isDefined());
  EXPECT_FLOAT_EQ(100.0f, result.unwrap());
}

TEST(StyleExpression, valid_multiply_number_by_number) {
  // calc(2 * 16) → 32. Both operands are <number>, result is length-compatible.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::multiply(1, 2),
      ExpressionNode::number(2.0f),
      ExpressionNode::number(16.0f),
  };
  auto result = evaluate(pool, 0, 0.0f);
  EXPECT_TRUE(result.isDefined());
  EXPECT_FLOAT_EQ(32.0f, result.unwrap());
}

TEST(StyleExpression, valid_percent_minus_number_multiply) {
  // calc(100% - 2 * 16) with ref=375 → 375 - 32 = 343. Regression guard that
  // the §3 number*number-as-length case still works after the stricter rule.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::subtract(1, 2), // [0]
      ExpressionNode::percent(100.0f), // [1]
      ExpressionNode::multiply(3, 4), // [2]
      ExpressionNode::number(2.0f), // [3]
      ExpressionNode::number(16.0f), // [4]
  };
  auto result = evaluate(pool, 0, 375.0f);
  EXPECT_TRUE(result.isDefined());
  EXPECT_FLOAT_EQ(343.0f, result.unwrap());
}

// §2 / FIX 2: min/max/clamp arguments must share a type. Mixing a <number>
// with a <length> among the arguments is IACVT.

TEST(StyleExpression, invalid_min_number_mixed_with_length) {
  // min(2, 50px) — <number> vs <length> → IACVT → undefined.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::min(1, 2),
      ExpressionNode::number(2.0f),
      ExpressionNode::value(50.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, invalid_max_number_mixed_with_length) {
  // max(2, 50px) — <number> vs <length> → IACVT → undefined.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::max(1, 2),
      ExpressionNode::number(2.0f),
      ExpressionNode::value(50.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, invalid_clamp_number_mixed_with_length) {
  // clamp(2, 50px, 100px) — first arg is a <number>, others are lengths →
  // IACVT → undefined.
  std::vector<ExpressionNode> pool = {
      ExpressionNode::clamp(1, 2, 3),
      ExpressionNode::number(2.0f),
      ExpressionNode::value(50.0f),
      ExpressionNode::value(100.0f),
  };
  EXPECT_FALSE(evaluate(pool, 0, 100.0f).isDefined());
}

TEST(StyleExpression, valid_min_all_numbers) {
  // min(2, 3) — both <number>, result is the <number> 2 (regression guard:
  // the same-type rule must not reject all-number argument lists).
  std::vector<ExpressionNode> pool = {
      ExpressionNode::min(1, 2),
      ExpressionNode::number(2.0f),
      ExpressionNode::number(3.0f),
  };
  auto result = evaluate(pool, 0, 0.0f);
  EXPECT_TRUE(result.isDefined());
  EXPECT_FLOAT_EQ(2.0f, result.unwrap());
}

TEST(StyleExpression, valid_clamp_all_lengths_and_percent) {
  // clamp(16px, 50%, 200px) with ref=100 → 50 (all non-number length/percent
  // types — same-type rule must accept this).
  std::vector<ExpressionNode> pool = {
      ExpressionNode::clamp(1, 2, 3),
      ExpressionNode::value(16.0f),
      ExpressionNode::percent(50.0f),
      ExpressionNode::value(200.0f),
  };
  auto result = evaluate(pool, 0, 100.0f);
  EXPECT_TRUE(result.isDefined());
  EXPECT_FLOAT_EQ(50.0f, result.unwrap());
}
