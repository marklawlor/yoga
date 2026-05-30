/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <yoga/Yoga.h>
#include <yoga/YGExpression.h>
#include <yoga/YGExpressionInternal.h>
#include <yoga/config/Config.h>
#include <yoga/style/StyleExpression.h>

#include <memory>

using namespace facebook::yoga;

namespace {

// Helper: build an expression node pool from a builder, interning env names
// against `config`, then evaluate it.
FloatOptional evalBuilder(
    YGExpressionRef expr,
    float ref,
    const Config* config) {
  auto pool = YGExpressionSerialise(expr);
  YGExpressionFree(expr);
  return evaluate(pool, /*rootIndex=*/0, ref, config);
}

} // namespace

// --- Parser acceptance / rejection ---

TEST(StyleEnv, parser_accepts_env_name) {
  YGExpressionRef e = YGExpressionParse("env(safe-area-inset-top)");
  ASSERT_NE(e, nullptr);
  YGExpressionFree(e);
}

TEST(StyleEnv, parser_accepts_env_name_with_fallback) {
  YGExpressionRef e = YGExpressionParse("env(my-inset, 16)");
  ASSERT_NE(e, nullptr);
  YGExpressionFree(e);
}

TEST(StyleEnv, parser_accepts_calc_with_env) {
  YGExpressionRef e = YGExpressionParse("calc(env(my-inset) + 8)");
  ASSERT_NE(e, nullptr);
  YGExpressionFree(e);
}

TEST(StyleEnv, parser_accepts_underscore_and_digits_in_name) {
  YGExpressionRef e = YGExpressionParse("env(_my_env-2)");
  ASSERT_NE(e, nullptr);
  YGExpressionFree(e);
}

TEST(StyleEnv, parser_rejects_env_missing_name) {
  EXPECT_EQ(YGExpressionParse("env()"), nullptr);
}

TEST(StyleEnv, parser_rejects_env_missing_paren) {
  EXPECT_EQ(YGExpressionParse("env(my-inset"), nullptr);
}

TEST(StyleEnv, parser_rejects_env_name_starting_with_digit) {
  EXPECT_EQ(YGExpressionParse("env(2foo)"), nullptr);
}

TEST(StyleEnv, parser_rejects_env_trailing_garbage) {
  EXPECT_EQ(YGExpressionParse("env(my-inset) px"), nullptr);
}

TEST(StyleEnv, parser_rejects_env_bad_fallback) {
  EXPECT_EQ(YGExpressionParse("env(my-inset, 16px)"), nullptr);
}

// --- Interning stability ---

TEST(StyleEnv, intern_same_name_same_id) {
  Config config{nullptr};
  uint16_t a = internEnvName("foo");
  uint16_t b = internEnvName("foo");
  EXPECT_EQ(a, b);
}

TEST(StyleEnv, intern_different_names_different_ids) {
  Config config{nullptr};
  uint16_t a = internEnvName("foo");
  uint16_t b = internEnvName("bar");
  EXPECT_NE(a, b);
}

TEST(StyleEnv, intern_does_not_bump_version) {
  Config config{nullptr};
  uint32_t before = config.getVersion();
  internEnvName("never-seen-before");
  EXPECT_EQ(config.getVersion(), before);
}

TEST(StyleEnv, copy_env_from_carries_values) {
  Config src{nullptr};
  src.setEnv("copied-name", FloatOptional{42.0f});
  Config dst{nullptr};
  EXPECT_FALSE(dst.getEnv("copied-name").isDefined());

  uint32_t before = dst.getVersion();
  dst.copyEnvFrom(src);
  EXPECT_EQ(dst.getEnv("copied-name").unwrap(), 42.0f);
  EXPECT_NE(dst.getVersion(), before); // value changed -> version bump

  // Copying an identical table is a no-op (no version churn).
  uint32_t after = dst.getVersion();
  dst.copyEnvFrom(src);
  EXPECT_EQ(dst.getVersion(), after);
}

// --- C API set/get/remove round-trip ---

TEST(StyleEnv, c_api_set_get_round_trip) {
  YGConfigRef config = YGConfigNew();
  YGConfigSetEnv(config, "my-inset", 24.0f);
  EXPECT_FLOAT_EQ(YGConfigGetEnv(config, "my-inset"), 24.0f);
  YGConfigFree(config);
}

TEST(StyleEnv, c_api_get_unset_is_undefined) {
  YGConfigRef config = YGConfigNew();
  EXPECT_TRUE(YGFloatIsUndefined(YGConfigGetEnv(config, "never-set")));
  YGConfigFree(config);
}

TEST(StyleEnv, c_api_remove_clears_value) {
  YGConfigRef config = YGConfigNew();
  YGConfigSetEnv(config, "my-inset", 24.0f);
  YGConfigRemoveEnv(config, "my-inset");
  EXPECT_TRUE(YGFloatIsUndefined(YGConfigGetEnv(config, "my-inset")));
  YGConfigFree(config);
}

TEST(StyleEnv, c_api_set_undefined_equals_remove) {
  YGConfigRef config = YGConfigNew();
  YGConfigSetEnv(config, "my-inset", 24.0f);
  YGConfigSetEnv(config, "my-inset", YGUndefined);
  EXPECT_TRUE(YGFloatIsUndefined(YGConfigGetEnv(config, "my-inset")));
  YGConfigFree(config);
}

TEST(StyleEnv, c_api_set_bumps_version) {
  YGConfigRef config = YGConfigNew();
  uint32_t before = resolveRef(config)->getVersion();
  YGConfigSetEnv(config, "my-inset", 24.0f);
  EXPECT_GT(resolveRef(config)->getVersion(), before);
  YGConfigFree(config);
}

// --- Defaults (decision 11) ---

TEST(StyleEnv, safe_area_insets_default_to_zero) {
  YGConfigRef config = YGConfigNew();
  EXPECT_FLOAT_EQ(YGConfigGetEnv(config, "safe-area-inset-top"), 0.0f);
  EXPECT_FLOAT_EQ(YGConfigGetEnv(config, "safe-area-inset-right"), 0.0f);
  EXPECT_FLOAT_EQ(YGConfigGetEnv(config, "safe-area-inset-bottom"), 0.0f);
  EXPECT_FLOAT_EQ(YGConfigGetEnv(config, "safe-area-inset-left"), 0.0f);
  YGConfigFree(config);
}

// --- evaluate(): set-value / fallback / IACVT ---

TEST(StyleEnv, eval_set_value_used) {
  Config config{nullptr};
  config.setEnv("my-inset", FloatOptional{42.0f});
  // env(my-inset) with no fallback
  YGExpressionRef e = YGExpressionEnv("my-inset", nullptr);
  EXPECT_FLOAT_EQ(evalBuilder(e, 100.0f, &config).unwrap(), 42.0f);
}

TEST(StyleEnv, eval_set_value_ignores_fallback) {
  Config config{nullptr};
  config.setEnv("my-inset", FloatOptional{42.0f});
  YGExpressionRef e =
      YGExpressionEnv("my-inset", YGExpressionValue(7.0f));
  EXPECT_FLOAT_EQ(evalBuilder(e, 100.0f, &config).unwrap(), 42.0f);
}

TEST(StyleEnv, eval_unset_uses_fallback) {
  Config config{nullptr};
  // my-inset never set
  YGExpressionRef e =
      YGExpressionEnv("my-inset", YGExpressionValue(7.0f));
  EXPECT_FLOAT_EQ(evalBuilder(e, 100.0f, &config).unwrap(), 7.0f);
}

TEST(StyleEnv, eval_unset_no_fallback_is_undefined) {
  Config config{nullptr};
  YGExpressionRef e = YGExpressionEnv("my-inset", nullptr);
  EXPECT_TRUE(evalBuilder(e, 100.0f, &config).isUndefined());
}

TEST(StyleEnv, eval_fallback_is_percent) {
  Config config{nullptr};
  // env(my-inset, 50%) unset -> 50% of ref 200 = 100
  YGExpressionRef e =
      YGExpressionEnv("my-inset", YGExpressionPercent(50.0f));
  EXPECT_FLOAT_EQ(evalBuilder(e, 200.0f, &config).unwrap(), 100.0f);
}

// --- Type walks (decision 6) ---

TEST(StyleEnv, eval_calc_env_plus_value) {
  Config config{nullptr};
  config.setEnv("my-inset", FloatOptional{10.0f});
  // calc(env(my-inset) + 8) = 18
  YGExpressionRef e = YGExpressionParse("calc(env(my-inset) + 8)");
  ASSERT_NE(e, nullptr);
  EXPECT_FLOAT_EQ(evalBuilder(e, 100.0f, &config).unwrap(), 18.0f);
}

TEST(StyleEnv, eval_calc_env_times_number_valid) {
  Config config{nullptr};
  config.setEnv("my-inset", FloatOptional{10.0f});
  // calc(env(my-inset) * 2) = 20 (env is length-typed, number * length OK)
  YGExpressionRef e = YGExpressionParse("calc(env(my-inset) * 2)");
  ASSERT_NE(e, nullptr);
  EXPECT_FLOAT_EQ(evalBuilder(e, 100.0f, &config).unwrap(), 20.0f);
}

TEST(StyleEnv, eval_calc_number_divided_by_env_is_iacvt) {
  Config config{nullptr};
  config.setEnv("my-inset", FloatOptional{10.0f});
  // calc(10 / env(my-inset)): env is not a valid (dimensionless) divisor.
  YGExpressionRef e = YGExpressionParse("calc(10 / env(my-inset))");
  ASSERT_NE(e, nullptr);
  EXPECT_TRUE(evalBuilder(e, 100.0f, &config).isUndefined());
}

TEST(StyleEnv, eval_calc_percent_minus_env) {
  Config config{nullptr};
  config.setEnv("my-inset", FloatOptional{20.0f});
  // calc(100% - env(my-inset)) with ref=200 -> 200 - 20 = 180
  YGExpressionRef e = YGExpressionParse("calc(100% - env(my-inset))");
  ASSERT_NE(e, nullptr);
  EXPECT_FLOAT_EQ(evalBuilder(e, 200.0f, &config).unwrap(), 180.0f);
}

// --- End-to-end layout: set-value path ---

TEST(StyleEnv, layout_width_from_env_set_value) {
  YGConfigRef config = YGConfigNew();
  YGConfigSetEnv(config, "panel-width", 120.0f);

  YGNodeRef root = YGNodeNewWithConfig(config);
  YGExpressionRef e = YGExpressionEnv("panel-width", nullptr);
  YGNodeStyleSetWidthExpression(root, e);
  YGExpressionFree(e);
  YGNodeStyleSetHeight(root, 50.0f);

  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(YGNodeLayoutGetWidth(root), 120.0f);

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

TEST(StyleEnv, layout_relayout_after_env_change) {
  YGConfigRef config = YGConfigNew();
  YGConfigSetEnv(config, "panel-width", 120.0f);

  YGNodeRef root = YGNodeNewWithConfig(config);
  YGExpressionRef e = YGExpressionEnv("panel-width", nullptr);
  YGNodeStyleSetWidthExpression(root, e);
  YGExpressionFree(e);
  YGNodeStyleSetHeight(root, 50.0f);

  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(YGNodeLayoutGetWidth(root), 120.0f);

  // Changing the env value bumps the config version; recalculating must
  // reflect the new value with no manual dirtying.
  YGConfigSetEnv(config, "panel-width", 200.0f);
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(YGNodeLayoutGetWidth(root), 200.0f);

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

// Non-dimension property path: padding env. Exercises the
// computeFlexStartPadding wrapper chain (distinct from the dimension path).
TEST(StyleEnv, layout_padding_from_env_set_value) {
  YGConfigRef config = YGConfigNew();
  YGConfigSetEnv(config, "pad", 8.0f);

  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 100.0f);
  YGNodeStyleSetHeight(root, 100.0f);
  YGExpressionRef e = YGExpressionEnv("pad", nullptr);
  YGNodeStyleSetPaddingExpression(root, YGEdgeLeft, e);
  YGExpressionFree(e);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(child, 10.0f);
  YGNodeStyleSetHeight(child, 10.0f);
  YGNodeInsertChild(root, child, 0);

  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  // The child is offset by the resolved left padding (8).
  EXPECT_FLOAT_EQ(YGNodeLayoutGetLeft(child), 8.0f);

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

// Fallback path used at layout time when the name is unset.
TEST(StyleEnv, layout_width_uses_fallback_when_unset) {
  YGConfigRef config = YGConfigNew();
  // "missing" is never set on the config.

  YGNodeRef root = YGNodeNewWithConfig(config);
  YGExpressionRef e = YGExpressionEnv("missing", YGExpressionValue(64.0f));
  YGNodeStyleSetWidthExpression(root, e);
  YGExpressionFree(e);
  YGNodeStyleSetHeight(root, 50.0f);

  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(YGNodeLayoutGetWidth(root), 64.0f);

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}

// Pre-populated safe-area defaults resolve to 0 in layout.
TEST(StyleEnv, layout_safe_area_default_is_zero) {
  YGConfigRef config = YGConfigNew();

  YGNodeRef root = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(root, 100.0f);
  YGNodeStyleSetHeight(root, 100.0f);
  YGExpressionRef e = YGExpressionEnv("safe-area-inset-left", nullptr);
  YGNodeStyleSetPaddingExpression(root, YGEdgeLeft, e);
  YGExpressionFree(e);

  YGNodeRef child = YGNodeNewWithConfig(config);
  YGNodeStyleSetWidth(child, 10.0f);
  YGNodeStyleSetHeight(child, 10.0f);
  YGNodeInsertChild(root, child, 0);

  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
  EXPECT_FLOAT_EQ(YGNodeLayoutGetLeft(child), 0.0f);

  YGNodeFreeRecursive(root);
  YGConfigFree(config);
}
