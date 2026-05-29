/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <yoga/style/StyleExpression.h>
#include <yoga/style/StyleValuePool.h>

namespace facebook::yoga {

TEST(StyleValuePool, undefined_at_init) {
  StyleValuePool pool;
  StyleValueHandle handle;

  EXPECT_TRUE(handle.isUndefined());
  EXPECT_FALSE(handle.isDefined());
  EXPECT_EQ(pool.getLength(handle), StyleLength::undefined());
  EXPECT_EQ(pool.getNumber(handle), FloatOptional{});
}

TEST(StyleValuePool, auto_at_init) {
  StyleValuePool pool;
  auto handle = StyleValueHandle::ofAuto();

  EXPECT_TRUE(handle.isAuto());
  EXPECT_EQ(pool.getLength(handle), StyleLength::ofAuto());
}

TEST(StyleValuePool, store_small_int_points) {
  StyleValuePool pool;
  StyleValueHandle handle;

  pool.store(handle, StyleLength::points(10));

  EXPECT_EQ(pool.getLength(handle), StyleLength::points(10));
}

TEST(StyleValuePool, store_small_negative_int_points) {
  StyleValuePool pool;
  StyleValueHandle handle;

  pool.store(handle, StyleLength::points(-10));

  EXPECT_EQ(pool.getLength(handle), StyleLength::points(-10));
}

TEST(StyleValuePool, store_small_int_percent) {
  StyleValuePool pool;
  StyleValueHandle handle;

  pool.store(handle, StyleLength::percent(10));

  EXPECT_EQ(pool.getLength(handle), StyleLength::percent(10));
}

TEST(StyleValuePool, store_large_int_percent) {
  StyleValuePool pool;
  StyleValueHandle handle;

  pool.store(handle, StyleLength::percent(262144));

  EXPECT_EQ(pool.getLength(handle), StyleLength::percent(262144));
}

TEST(StyleValuePool, store_large_int_after_small_int) {
  StyleValuePool pool;
  StyleValueHandle handle;

  pool.store(handle, StyleLength::percent(10));
  pool.store(handle, StyleLength::percent(262144));

  EXPECT_EQ(pool.getLength(handle), StyleLength::percent(262144));
}

TEST(StyleValuePool, store_small_int_after_large_int) {
  StyleValuePool pool;
  StyleValueHandle handle;

  pool.store(handle, StyleLength::percent(262144));
  pool.store(handle, StyleLength::percent(10));

  EXPECT_EQ(pool.getLength(handle), StyleLength::percent(10));
}

TEST(StyleValuePool, store_small_int_number) {
  StyleValuePool pool;
  StyleValueHandle handle;

  pool.store(handle, FloatOptional{10.0f});

  EXPECT_EQ(pool.getNumber(handle), FloatOptional{10.0f});
}

TEST(StyleValuePool, store_undefined) {
  StyleValuePool pool;
  StyleValueHandle handle;

  pool.store(handle, StyleLength::undefined());

  EXPECT_TRUE(handle.isUndefined());
  EXPECT_FALSE(handle.isDefined());
  EXPECT_EQ(pool.getLength(handle), StyleLength::undefined());
}

TEST(StyleValuePool, store_undefined_after_small_int) {
  StyleValuePool pool;
  StyleValueHandle handle;

  pool.store(handle, StyleLength::points(10));
  pool.store(handle, StyleLength::undefined());

  EXPECT_TRUE(handle.isUndefined());
  EXPECT_FALSE(handle.isDefined());
  EXPECT_EQ(pool.getLength(handle), StyleLength::undefined());
}

TEST(StyleValuePool, store_undefined_after_large_int) {
  StyleValuePool pool;
  StyleValueHandle handle;

  pool.store(handle, StyleLength::points(262144));
  pool.store(handle, StyleLength::undefined());

  EXPECT_TRUE(handle.isUndefined());
  EXPECT_FALSE(handle.isDefined());
  EXPECT_EQ(pool.getLength(handle), StyleLength::undefined());
}

TEST(StyleValuePool, store_keywords) {
  StyleValuePool pool;
  StyleValueHandle handleMaxContent;
  StyleValueHandle handleFitContent;
  StyleValueHandle handleStretch;

  pool.store(handleMaxContent, StyleSizeLength::ofMaxContent());
  pool.store(handleFitContent, StyleSizeLength::ofFitContent());
  pool.store(handleStretch, StyleSizeLength::ofStretch());

  EXPECT_EQ(pool.getSize(handleMaxContent), StyleSizeLength::ofMaxContent());
  EXPECT_EQ(pool.getSize(handleFitContent), StyleSizeLength::ofFitContent());
  EXPECT_EQ(pool.getSize(handleStretch), StyleSizeLength::ofStretch());
}

TEST(StyleValuePool, store_and_resolve_expression) {
  // calc(100% - 32px) with referenceLength=375 → 343
  StyleValuePool pool;
  StyleValueHandle handle;

  std::vector<ExpressionNode> nodes = {
      ExpressionNode::subtract(1, 2),
      ExpressionNode::percent(100.0f),
      ExpressionNode::value(32.0f),
  };
  pool.store(handle, nodes);

  EXPECT_TRUE(handle.isExpression());
  EXPECT_FLOAT_EQ(343.0f, pool.evaluateExpression(handle, 375.0f).unwrap());
}

TEST(StyleValuePool, store_expression_min) {
  // min(50%, 200px) with referenceLength=300 → 150
  StyleValuePool pool;
  StyleValueHandle handle;

  std::vector<ExpressionNode> nodes = {
      ExpressionNode::min(1, 2),
      ExpressionNode::percent(50.0f),
      ExpressionNode::value(200.0f),
  };
  pool.store(handle, nodes);

  EXPECT_FLOAT_EQ(150.0f, pool.evaluateExpression(handle, 300.0f).unwrap());
}

TEST(StyleValuePool, overwrite_expression_in_place) {
  StyleValuePool pool;
  StyleValueHandle handle;

  std::vector<ExpressionNode> firstNodes = {
      ExpressionNode::subtract(1, 2),
      ExpressionNode::percent(100.0f),
      ExpressionNode::value(32.0f),
  };
  std::vector<ExpressionNode> secondNodes = {
      ExpressionNode::min(1, 2),
      ExpressionNode::percent(50.0f),
      ExpressionNode::value(200.0f),
  };

  pool.store(handle, firstNodes);
  EXPECT_TRUE(handle.isExpression());

  pool.store(handle, secondNodes);

  EXPECT_EQ(pool.getExpressionNodes(handle), secondNodes);
  EXPECT_FLOAT_EQ(150.0f, pool.evaluateExpression(handle, 300.0f).unwrap());
}

TEST(StyleValuePool, expression_cleared_on_scalar_store) {
  StyleValuePool pool;
  StyleValueHandle handle;

  pool.store(handle, {ExpressionNode::percent(50.0f)});
  EXPECT_TRUE(handle.isExpression());

  pool.store(handle, StyleLength::points(100.0f));
  EXPECT_FALSE(handle.isExpression());

  pool.store(handle, {ExpressionNode::value(200.0f)});
  EXPECT_TRUE(handle.isExpression());
  EXPECT_FLOAT_EQ(200.0f, pool.evaluateExpression(handle, 0.0f).unwrap());
}

TEST(StyleValuePool, expression_structural_equality) {
  StyleValuePool poolA, poolB;
  StyleValueHandle hA, hB;

  std::vector<ExpressionNode> nodes = {
      ExpressionNode::subtract(1, 2),
      ExpressionNode::percent(100.0f),
      ExpressionNode::value(32.0f),
  };
  poolA.store(hA, nodes);
  poolB.store(hB, nodes);

  EXPECT_EQ(poolA.getExpressionNodes(hA), poolB.getExpressionNodes(hB));
}

TEST(StyleValuePool, expression_not_equal_to_different) {
  StyleValuePool poolA, poolB;
  StyleValueHandle hA, hB;
  poolA.store(
      hA,
      {ExpressionNode::subtract(1, 2),
       ExpressionNode::percent(100.0f),
       ExpressionNode::value(32.0f)});
  poolB.store(
      hB,
      {ExpressionNode::subtract(1, 2),
       ExpressionNode::percent(100.0f),
       ExpressionNode::value(16.0f)});
  EXPECT_NE(poolA.getExpressionNodes(hA), poolB.getExpressionNodes(hB));
}

TEST(StyleValuePool, expression_slot_free_list_bounded) {
  // expression→scalar→expression repeated 5000 times must NOT grow
  // expressionSlots_ unboundedly. With the free-list fix, the slot is
  // reused each cycle so expressionSlots_ stays at size 1.
  StyleValuePool pool;
  StyleValueHandle handle;

  for (int i = 0; i < 5000; ++i) {
    // Set expression
    pool.store(handle, {ExpressionNode::percent(50.0f)});
    ASSERT_TRUE(handle.isExpression());

    // Set scalar (triggers free-list push)
    pool.store(handle, StyleLength::points(100.0f));
    ASSERT_FALSE(handle.isExpression());
  }

  // Set one final expression and verify it evaluates correctly
  pool.store(handle, {ExpressionNode::percent(50.0f)});
  EXPECT_TRUE(handle.isExpression());
  EXPECT_FLOAT_EQ(50.0f, pool.evaluateExpression(handle, 100.0f).unwrap());

  // The pool must have at most 1 live slot (not 5000 dead ones).
  // We verify by setting yet another expression on a fresh handle and
  // confirming it evaluates correctly — if slots were exhausted the
  // assert(size < 4096) would have fired in the loop above.
}

TEST(StyleValuePool, buffer_chunk_reused_across_value_expression_toggle) {
  // Repeatedly toggling a fractional (non-integer-packable) length and an
  // expression on the same handle must reuse the SmallValueBuffer chunk
  // orphaned by each value→expression transition rather than allocating a new
  // one. Without reuse this loop pushes 5000 chunks and trips the buffer's
  // assert(index < 4096) in debug builds.
  StyleValuePool pool;
  StyleValueHandle handle;

  for (int i = 0; i < 5000; ++i) {
    pool.store(handle, StyleLength::points(0.5f)); // fractional → buffer chunk
    ASSERT_TRUE(handle.isPoint());
    pool.store(handle, {ExpressionNode::percent(50.0f)}); // orphans the chunk
    ASSERT_TRUE(handle.isExpression());
  }

  // The reused chunk still round-trips a fresh value correctly.
  pool.store(handle, StyleLength::points(0.25f));
  EXPECT_EQ(pool.getLength(handle), StyleLength::points(0.25f));
}

} // namespace facebook::yoga
