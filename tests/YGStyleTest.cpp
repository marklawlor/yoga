/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <yoga/YGExpression.h>
#include <yoga/Yoga.h>
#include <string>

TEST(YogaTest, copy_style_same) {
  YGNodeRef node0 = YGNodeNew();
  YGNodeRef node1 = YGNodeNew();

  YGNodeCopyStyle(node0, node1);

  YGNodeFree(node0);
  YGNodeFree(node1);
}

TEST(YogaTest, copy_style_modified) {
  YGNodeRef node0 = YGNodeNew();
  ASSERT_EQ(YGFlexDirectionColumn, YGNodeStyleGetFlexDirection(node0));
  ASSERT_FALSE(YGNodeStyleGetMaxHeight(node0).unit != YGUnitUndefined);

  YGNodeRef node1 = YGNodeNew();
  YGNodeStyleSetFlexDirection(node1, YGFlexDirectionRow);
  YGNodeStyleSetMaxHeight(node1, 10);

  YGNodeCopyStyle(node0, node1);
  ASSERT_EQ(YGFlexDirectionRow, YGNodeStyleGetFlexDirection(node0));
  ASSERT_FLOAT_EQ(10, YGNodeStyleGetMaxHeight(node0).value);

  YGNodeFree(node0);
  YGNodeFree(node1);
}

TEST(YogaTest, copy_style_modified_same) {
  YGNodeRef node0 = YGNodeNew();
  YGNodeStyleSetFlexDirection(node0, YGFlexDirectionRow);
  YGNodeStyleSetMaxHeight(node0, 10);
  YGNodeCalculateLayout(node0, YGUndefined, YGUndefined, YGDirectionLTR);

  YGNodeRef node1 = YGNodeNew();
  YGNodeStyleSetFlexDirection(node1, YGFlexDirectionRow);
  YGNodeStyleSetMaxHeight(node1, 10);

  YGNodeCopyStyle(node0, node1);

  YGNodeFree(node0);
  YGNodeFree(node1);
}

TEST(YogaTest, initialise_flexShrink_flexGrow) {
  YGNodeRef node0 = YGNodeNew();
  YGNodeStyleSetFlexShrink(node0, 1);
  ASSERT_EQ(1, YGNodeStyleGetFlexShrink(node0));

  YGNodeStyleSetFlexShrink(node0, YGUndefined);
  YGNodeStyleSetFlexGrow(node0, 3);
  ASSERT_EQ(
      0,
      YGNodeStyleGetFlexShrink(
          node0)); // Default value is Zero, if flex shrink is not defined
  ASSERT_EQ(3, YGNodeStyleGetFlexGrow(node0));

  YGNodeStyleSetFlexGrow(node0, YGUndefined);
  YGNodeStyleSetFlexShrink(node0, 3);
  ASSERT_EQ(
      0,
      YGNodeStyleGetFlexGrow(
          node0)); // Default value is Zero, if flex grow is not defined
  ASSERT_EQ(3, YGNodeStyleGetFlexShrink(node0));
  YGNodeFree(node0);
}

TEST(YogaTest, public_api_expression_width) {
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 375.0f);
  YGNodeStyleSetHeight(root, 100.0f);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);

  // width: calc(100% - 32px) in a 375px parent → 343px
  YGExpressionRef expr = YGExpressionSubtract(
      YGExpressionPercent(100.0f), YGExpressionValue(32.0f));
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);

  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);

  EXPECT_FLOAT_EQ(343.0f, YGNodeLayoutGetWidth(child));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, public_api_expression_min) {
  // width: min(50%, 200px) in a 300px parent → 150px
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 300.0f);
  YGNodeStyleSetHeight(root, 100.0f);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr =
      YGExpressionMin(YGExpressionPercent(50.0f), YGExpressionValue(200.0f));
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);

  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(150.0f, YGNodeLayoutGetWidth(child));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, public_api_expression_get_width_returns_undefined) {
  // After setting an expression width, YGNodeStyleGetWidth must return
  // YGValueUndefined — the expression is evaluated during layout, not by the
  // getter.
  YGConfigRef config = YGConfigNew();
  YGNodeRef node = YGNodeNewWithConfig(config);
  YGExpressionRef expr = YGExpressionPercent(50.0f);
  YGNodeStyleSetWidthExpression(node, expr);
  YGExpressionFree(expr);

  YGValue w = YGNodeStyleGetWidth(node);
  EXPECT_EQ(YGUnitUndefined, w.unit);

  YGNodeFree(node);
  YGConfigFree(config);
}

TEST(YogaTest, public_api_expression_padding) {
  // padding-left: calc(5% + 8px) in a 200px parent → 18px
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 200.0f);
  YGNodeStyleSetHeight(root, 100.0f);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(child, 100.0f);
  YGNodeStyleSetHeight(child, 100.0f);
  YGExpressionRef expr =
      YGExpressionAdd(YGExpressionPercent(5.0f), YGExpressionValue(8.0f));
  YGNodeStyleSetPaddingExpression(child, YGEdgeLeft, expr);
  YGExpressionFree(expr);

  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(18.0f, YGNodeLayoutGetPadding(child, YGEdgeLeft));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_calc_subtract) {
  // calc(100% - 32) in a 375px parent → 343px
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 375.0f);
  YGNodeStyleSetHeight(root, 100.0f);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("calc(100% - 32)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);

  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(343.0f, YGNodeLayoutGetWidth(child));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_min) {
  // min(50%, 200) in a 300px parent → 150px
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 300.0f);
  YGNodeStyleSetHeight(root, 100.0f);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("min(50%, 200)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);

  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(150.0f, YGNodeLayoutGetWidth(child));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_clamp) {
  // clamp(16, 5%, 48) in a 200px parent → 16px (5% = 10, below min)
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 200.0f);
  YGNodeStyleSetHeight(root, 100.0f);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("clamp(16, 5%, 48)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);

  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(16.0f, YGNodeLayoutGetWidth(child));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_nested) {
  // calc(min(50%, 200) - 16) in a 300px parent → 134px
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 300.0f);
  YGNodeStyleSetHeight(root, 100.0f);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("calc(min(50%, 200) - 16)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);

  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(134.0f, YGNodeLayoutGetWidth(child));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_returns_null_for_px_units) {
  // CSS px units are not valid in Yoga expressions — return NULL
  EXPECT_EQ(nullptr, YGExpressionParse("calc(100% - 32px)"));
}

TEST(YogaTest, parse_expression_returns_null_for_var) {
  EXPECT_EQ(nullptr, YGExpressionParse("var(--spacing)"));
}

TEST(YogaTest, parse_expression_returns_null_for_empty) {
  EXPECT_EQ(nullptr, YGExpressionParse(""));
  EXPECT_EQ(nullptr, YGExpressionParse(nullptr));
}

TEST(YogaTest, parse_expression_add) {
  // calc(50% + 16) in a 200px parent → 116px
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 200.0f);
  YGNodeStyleSetHeight(root, 100.0f);
  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("calc(50% + 16)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);
  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(116.0f, YGNodeLayoutGetWidth(child));
  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_trailing_garbage_returns_null) {
  EXPECT_EQ(nullptr, YGExpressionParse("50% hello"));
  EXPECT_EQ(nullptr, YGExpressionParse("calc(100% - 32) extra"));
}

TEST(YogaTest, parse_expression_multiply) {
  // calc(50% * 2) in a 100px parent → 100px (50% * 2 = 100% of 100px)
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 100.0f);
  YGNodeStyleSetHeight(root, 100.0f);
  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("calc(50% * 2)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);
  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(100.0f, YGNodeLayoutGetWidth(child));
  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_divide) {
  // calc(100% / 4) in a 200px parent → 50px
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 200.0f);
  YGNodeStyleSetHeight(root, 100.0f);
  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("calc(100% / 4)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);
  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(50.0f, YGNodeLayoutGetWidth(child));
  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_leading_dot_decimal) {
  // CSS allows leading-dot decimals: calc(.5 * 100%) in 200px parent → 100px
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 200.0f);
  YGNodeStyleSetHeight(root, 100.0f);
  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("calc(.5 * 100%)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);
  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(100.0f, YGNodeLayoutGetWidth(child));
  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_percent_minus_multiply) {
  // calc(100% - 2 * 16) in a 375px parent → 343px. A multiply of bare numbers
  // is a length-compatible value, so it must combine with the percentage rather
  // than being rejected as <number> - <length>.
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 375.0f);
  YGNodeStyleSetHeight(root, 100.0f);
  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("calc(100% - 2 * 16)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);
  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(343.0f, YGNodeLayoutGetWidth(child));
  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_multiply_plus_value) {
  // calc(2 * 3 + 5) → 11 (parent-independent), usable as a length.
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 100.0f);
  YGNodeStyleSetHeight(root, 100.0f);
  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("calc(2 * 3 + 5)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);
  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(11.0f, YGNodeLayoutGetWidth(child));
  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_rejects_excessive_depth) {
  // Deeply nested input must fail to parse rather than overflow the stack.
  std::string deep;
  for (int i = 0; i < 200; i++) {
    deep += "calc(";
  }
  deep += "0";
  for (int i = 0; i < 200; i++) {
    deep += ")";
  }
  EXPECT_EQ(nullptr, YGExpressionParse(deep.c_str()));
}

TEST(YogaTest, parse_expression_rejects_excessive_length) {
  // Oversized input must fail to parse rather than overflow the uint16 node
  // index used when flattening the expression tree.
  std::string longExpr = "1";
  for (int i = 0; i < 9000; i++) {
    longExpr += "+1";
  }
  EXPECT_EQ(nullptr, YGExpressionParse(longExpr.c_str()));
}

TEST(YogaTest, public_api_expression_position_left) {
  // position:absolute; left: calc(10% + 5) in a 200px parent → 25px
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 200.0f);
  YGNodeStyleSetHeight(root, 200.0f);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetPositionType(child, YGPositionTypeAbsolute);
  YGNodeStyleSetWidth(child, 50.0f);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr =
      YGExpressionAdd(YGExpressionPercent(10.0f), YGExpressionValue(5.0f));
  YGNodeStyleSetPositionExpression(child, YGEdgeLeft, expr);
  YGExpressionFree(expr);

  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(25.0f, YGNodeLayoutGetLeft(child));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, public_api_expression_position_rtl_start) {
  // RTL maps the inline-start edge to the physical right edge. With a 200px
  // container and a 50px child, start:calc(10% + 5)=25 → left = 200-50-25 = 125.
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 200.0f);
  YGNodeStyleSetHeight(root, 200.0f);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetPositionType(child, YGPositionTypeAbsolute);
  YGNodeStyleSetWidth(child, 50.0f);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr =
      YGExpressionAdd(YGExpressionPercent(10.0f), YGExpressionValue(5.0f));
  YGNodeStyleSetPositionExpression(child, YGEdgeStart, expr);
  YGExpressionFree(expr);

  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionRTL);
  EXPECT_FLOAT_EQ(125.0f, YGNodeLayoutGetLeft(child));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, public_api_expression_gap) {
  // column-gap: calc(4 + 6)=10 between two 50px row items → second item x = 60
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
  YGNodeStyleSetWidth(root, 200.0f);
  YGNodeStyleSetHeight(root, 100.0f);

  YGNodeRef child0 = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(child0, 50.0f);
  YGNodeStyleSetHeight(child0, 50.0f);
  YGNodeRef child1 = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(child1, 50.0f);
  YGNodeStyleSetHeight(child1, 50.0f);

  YGExpressionRef expr =
      YGExpressionAdd(YGExpressionValue(4.0f), YGExpressionValue(6.0f));
  YGNodeStyleSetGapExpression(root, YGGutterColumn, expr);
  YGExpressionFree(expr);

  YGNodeInsertChild(root, child0, 0);
  YGNodeInsertChild(root, child1, 1);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(60.0f, YGNodeLayoutGetLeft(child1));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, public_api_expression_min_height) {
  // min-height: calc(25% + 0) in a 200px-tall parent → 50px; child has no
  // content/height, so it is stretched up to the min.
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 100.0f);
  YGNodeStyleSetHeight(root, 200.0f);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("calc(25% + 0)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetMinHeightExpression(child, expr);
  YGExpressionFree(expr);

  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(50.0f, YGNodeLayoutGetHeight(child));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, public_api_expression_max_height) {
  // max-height: calc(10% + 0) in a 200px-tall parent → 20px; clamps the
  // explicit 100px height down to 20.
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 100.0f);
  YGNodeStyleSetHeight(root, 200.0f);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(child, 50.0f);
  YGNodeStyleSetHeight(child, 100.0f);
  YGExpressionRef expr = YGExpressionParse("calc(10% + 0)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetMaxHeightExpression(child, expr);
  YGExpressionFree(expr);

  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(20.0f, YGNodeLayoutGetHeight(child));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, public_api_expression_null_clears) {
  // Passing a null expression clears the property and re-dirties the node.
  YGConfigRef config = YGConfigNew();
  YGNodeRef node = YGNodeNewWithConfig(config);
  YGExpressionRef expr = YGExpressionPercent(50.0f);
  YGNodeStyleSetWidthExpression(node, expr);
  YGExpressionFree(expr);
  YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
  ASSERT_FALSE(YGNodeIsDirty(node));

  YGNodeStyleSetWidthExpression(node, nullptr);
  EXPECT_TRUE(YGNodeIsDirty(node));
  EXPECT_EQ(YGUnitUndefined, YGNodeStyleGetWidth(node).unit);

  YGNodeFree(node);
  YGConfigFree(config);
}

TEST(YogaTest, public_api_expression_set_equal_not_dirty) {
  // Re-setting an equal expression must not re-dirty the node.
  YGConfigRef config = YGConfigNew();
  YGNodeRef node = YGNodeNewWithConfig(config);
  YGExpressionRef expr0 = YGExpressionParse("calc(50% + 16)");
  ASSERT_NE(nullptr, expr0);
  YGNodeStyleSetWidthExpression(node, expr0);
  YGExpressionFree(expr0);
  YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
  ASSERT_FALSE(YGNodeIsDirty(node));

  YGExpressionRef expr1 = YGExpressionParse("calc(50% + 16)");
  ASSERT_NE(nullptr, expr1);
  YGNodeStyleSetWidthExpression(node, expr1);
  YGExpressionFree(expr1);
  EXPECT_FALSE(YGNodeIsDirty(node));

  YGNodeFree(node);
  YGConfigFree(config);
}

TEST(YogaTest, public_api_expression_min_max_equal_locks_dimension) {
  // When min-width == max-width are the same expression, the dimension is
  // locked and treated as definite — exactly like the equivalent point case
  // (min-width:100; max-width:100). With aspect-ratio 2 that makes the height
  // derive from the definite width (100 / 2 = 50) rather than being
  // content-sized.
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
  YGNodeStyleSetWidth(root, 200.0f);
  YGNodeStyleSetHeight(root, 200.0f);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetAspectRatio(child, 2.0f);
  // min-width == max-width == calc(50% + 0) == 100 in a 200px parent.
  YGExpressionRef minExpr =
      YGExpressionAdd(YGExpressionPercent(50.0f), YGExpressionValue(0.0f));
  YGExpressionRef maxExpr =
      YGExpressionAdd(YGExpressionPercent(50.0f), YGExpressionValue(0.0f));
  YGNodeStyleSetMinWidthExpression(child, minExpr);
  YGNodeStyleSetMaxWidthExpression(child, maxExpr);
  YGExpressionFree(minExpr);
  YGExpressionFree(maxExpr);

  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(100.0f, YGNodeLayoutGetWidth(child));
  EXPECT_FLOAT_EQ(50.0f, YGNodeLayoutGetHeight(child));

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_preserves_fractional_value) {
  // Regression guard for the full-lexeme strtof check: 12.5 must not be
  // truncated to 12. calc(12.5% + 0) in a 200px parent → 25px (12 would give
  // 24).
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 200.0f);
  YGNodeStyleSetHeight(root, 100.0f);
  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("calc(12.5% + 0)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);
  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(25.0f, YGNodeLayoutGetWidth(child));
  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, expression_percent_against_indefinite_parent_is_undefined) {
  // §8: a percentage-bearing expression resolved against an INDEFINITE parent
  // dimension behaves like a plain percentage — it resolves to undefined and
  // does NOT make the node definite. Here the root has no width (the available
  // width passed to layout is also undefined), so width: calc(100% - 32)
  // cannot resolve and the child width must fall back to its content size (0),
  // not 343 or any other definite value derived from a phantom parent width.
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  // Row container so the child's main axis (width) is content-sized when the
  // expression does not resolve to a definite value.
  YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
  YGNodeStyleSetAlignItems(root, YGAlignFlexStart);
  YGNodeStyleSetHeight(root, 100.0f);
  // Deliberately no width on root.
  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("calc(100% - 32)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);
  YGNodeInsertChild(root, child, 0);
  // Available width is undefined → parent width is indefinite.
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  // The percentage cannot resolve; the node is not made definite by it.
  EXPECT_FLOAT_EQ(0.0f, YGNodeLayoutGetWidth(child));
  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(YogaTest, parse_expression_unbalanced_parens_returns_null) {
  // Missing closing paren must fail to parse (and the setter would clear).
  EXPECT_EQ(nullptr, YGExpressionParse("calc(100% - 32"));
  EXPECT_EQ(nullptr, YGExpressionParse("min(50%, 200"));
}

TEST(YogaTest, parse_expression_empty_or_short_function_args_return_null) {
  // Functions with too few arguments are invalid.
  EXPECT_EQ(nullptr, YGExpressionParse("min()"));
  EXPECT_EQ(nullptr, YGExpressionParse("min(50%)"));
  EXPECT_EQ(nullptr, YGExpressionParse("max()"));
  EXPECT_EQ(nullptr, YGExpressionParse("clamp(1,2)"));
}

TEST(YogaTest, parse_expression_negative_literal) {
  // calc(-8 + 16) → 8. Negative literals are supported; the leading minus is a
  // sign on the number, not a binary subtract. width: calc(-8 + 16) = 8.
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 200.0f);
  YGNodeStyleSetHeight(root, 100.0f);
  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetHeight(child, 50.0f);
  YGExpressionRef expr = YGExpressionParse("calc(-8 + 16)");
  ASSERT_NE(nullptr, expr);
  YGNodeStyleSetWidthExpression(child, expr);
  YGExpressionFree(expr);
  YGNodeInsertChild(root, child, 0);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(8.0f, YGNodeLayoutGetWidth(child));
  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}
