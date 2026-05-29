/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

#include <yoga/numeric/FloatOptional.h>
#include <yoga/style/SmallValueBuffer.h>
#include <yoga/style/StyleExpression.h>
#include <yoga/style/StyleLength.h>
#include <yoga/style/StyleSizeLength.h>
#include <yoga/style/StyleValueHandle.h>

namespace facebook::yoga {

/**
 * StyleValuePool allows compact storage for a sparse collection of assigned
 * lengths and numbers. Values are referred to using StyleValueHandle. In most
 * cases StyleValueHandle can embed the value directly, but if not, the value is
 * stored within a buffer provided by the pool. The pool contains a fixed number
 * of inline slots before falling back to heap allocating additional slots.
 */
class StyleValuePool {
 public:
  void store(StyleValueHandle& handle, StyleLength length) {
    releaseExpressionSlot(handle);
    if (length.isUndefined()) {
      handle.setType(StyleValueHandle::Type::Undefined);
    } else if (length.isAuto()) {
      handle.setType(StyleValueHandle::Type::Auto);
    } else {
      auto type = length.isPoints() ? StyleValueHandle::Type::Point
                                    : StyleValueHandle::Type::Percent;
      storeValue(handle, length.value().unwrap(), type);
    }
  }

  void store(StyleValueHandle& handle, StyleSizeLength sizeValue) {
    releaseExpressionSlot(handle);
    if (sizeValue.isUndefined()) {
      handle.setType(StyleValueHandle::Type::Undefined);
    } else if (sizeValue.isAuto()) {
      handle.setType(StyleValueHandle::Type::Auto);
    } else if (sizeValue.isMaxContent()) {
      storeKeyword(handle, StyleValueHandle::Keyword::MaxContent);
    } else if (sizeValue.isStretch()) {
      storeKeyword(handle, StyleValueHandle::Keyword::Stretch);
    } else if (sizeValue.isFitContent()) {
      storeKeyword(handle, StyleValueHandle::Keyword::FitContent);
    } else {
      auto type = sizeValue.isPoints() ? StyleValueHandle::Type::Point
                                       : StyleValueHandle::Type::Percent;
      storeValue(handle, sizeValue.value().unwrap(), type);
    }
  }

  void store(StyleValueHandle& handle, FloatOptional number) {
    releaseExpressionSlot(handle);
    if (number.isUndefined()) {
      handle.setType(StyleValueHandle::Type::Undefined);
    } else {
      storeValue(handle, number.unwrap(), StyleValueHandle::Type::Number);
    }
  }

  void store(StyleValueHandle& handle, std::vector<ExpressionNode> nodes);

  FloatOptional evaluateExpression(
      StyleValueHandle handle,
      float referenceLength,
      const Config* config = nullptr) const;

  const std::vector<ExpressionNode>& getExpressionNodes(
      StyleValueHandle handle) const;

  // Structural (conservative) over-approximation: returns true if the
  // expression tree contains ANY Percent leaf, even when the expression
  // resolves to a parent-independent constant (e.g. calc(0 * 50%) or
  // calc(50% - 50%)). It does not evaluate whether the percentage actually
  // affects the result. The sole caller uses it to gate the "loose percentage"
  // cross-axis measurement path, so a false positive only yields a suboptimal
  // sizing-mode choice (MaxContent vs StretchFit), never a wrong value.
  bool expressionContainsPercent(StyleValueHandle handle) const {
    if (!handle.isExpression()) return false;
    for (const auto& node : expressionSlots_[handle.value()]) {
      if (node.kind == ExpressionNode::Kind::Percent) return true;
    }
    return false;
  }

  bool clearExpression(StyleValueHandle& handle) {
    if (!handle.isExpression()) return false;
    store(handle, StyleLength::undefined());
    return true;
  }

  bool clearExpressionSize(StyleValueHandle& handle) {
    if (!handle.isExpression()) return false;
    store(handle, StyleSizeLength::undefined());
    return true;
  }

  StyleLength getLength(StyleValueHandle handle) const {
    if (handle.isUndefined()) {
      return StyleLength::undefined();
    } else if (handle.isAuto()) {
      return StyleLength::ofAuto();
    } else {
      if (handle.isExpression()) {
        return StyleLength::undefined();
      }
      assert(
          handle.type() == StyleValueHandle::Type::Point ||
          handle.type() == StyleValueHandle::Type::Percent);
      float value = (handle.isValueIndexed())
          ? std::bit_cast<float>(buffer_.get32(handle.value()))
          : unpackInlineInteger(handle.value());

      return handle.type() == StyleValueHandle::Type::Point
          ? StyleLength::points(value)
          : StyleLength::percent(value);
    }
  }

  StyleSizeLength getSize(StyleValueHandle handle) const {
    if (handle.isUndefined()) {
      return StyleSizeLength::undefined();
    } else if (handle.isAuto()) {
      return StyleSizeLength::ofAuto();
    } else if (handle.isKeyword(StyleValueHandle::Keyword::MaxContent)) {
      return StyleSizeLength::ofMaxContent();
    } else if (handle.isKeyword(StyleValueHandle::Keyword::FitContent)) {
      return StyleSizeLength::ofFitContent();
    } else if (handle.isKeyword(StyleValueHandle::Keyword::Stretch)) {
      return StyleSizeLength::ofStretch();
    } else {
      if (handle.isExpression()) {
        return StyleSizeLength::undefined();
      }
      assert(
          handle.type() == StyleValueHandle::Type::Point ||
          handle.type() == StyleValueHandle::Type::Percent);
      float value = (handle.isValueIndexed())
          ? std::bit_cast<float>(buffer_.get32(handle.value()))
          : unpackInlineInteger(handle.value());

      return handle.type() == StyleValueHandle::Type::Point
          ? StyleSizeLength::points(value)
          : StyleSizeLength::percent(value);
    }
  }

  FloatOptional getNumber(StyleValueHandle handle) const {
    if (handle.isUndefined()) {
      return FloatOptional{};
    } else {
      assert(handle.type() == StyleValueHandle::Type::Number);
      float value = (handle.isValueIndexed())
          ? std::bit_cast<float>(buffer_.get32(handle.value()))
          : unpackInlineInteger(handle.value());
      return FloatOptional{value};
    }
  }

  float getStoredValue(StyleValueHandle handle) const {
    assert(
        handle.type() == StyleValueHandle::Type::Point ||
        handle.type() == StyleValueHandle::Type::Percent ||
        handle.type() == StyleValueHandle::Type::Number);
    return handle.isValueIndexed()
        ? std::bit_cast<float>(buffer_.get32(handle.value()))
        : unpackInlineInteger(handle.value());
  }

 private:
  void releaseExpressionSlot(StyleValueHandle handle) {
    if (!handle.isExpression()) return;
    uint16_t slot = handle.value();
    expressionSlots_[slot].clear();
    expressionSlots_[slot].shrink_to_fit();
    freeSlots_.push_back(slot);
  }

  void storeValue(
      StyleValueHandle& handle,
      float value,
      StyleValueHandle::Type type) {
    handle.setType(type);

    if (handle.isValueIndexed()) {
      auto newIndex =
          buffer_.replace(handle.value(), std::bit_cast<uint32_t>(value));
      handle.setValue(newIndex);
    } else if (isIntegerPackable(value)) {
      handle.setValue(packInlineInteger(value));
    } else if (!freeBufferChunks_.empty()) {
      // Reuse a buffer chunk orphaned by a previous value→expression
      // transition rather than growing the buffer.
      uint16_t reusedIndex = freeBufferChunks_.back();
      freeBufferChunks_.pop_back();
      auto newIndex =
          buffer_.replace(reusedIndex, std::bit_cast<uint32_t>(value));
      handle.setValue(newIndex);
      handle.setValueIsIndexed();
    } else {
      auto newIndex = buffer_.push(std::bit_cast<uint32_t>(value));
      handle.setValue(newIndex);
      handle.setValueIsIndexed();
    }
  }

  void storeKeyword(
      StyleValueHandle& handle,
      StyleValueHandle::Keyword keyword) {
    handle.setType(StyleValueHandle::Type::Keyword);

    if (handle.isValueIndexed()) {
      auto newIndex =
          buffer_.replace(handle.value(), static_cast<uint32_t>(keyword));
      handle.setValue(newIndex);
    } else {
      handle.setValue(static_cast<uint16_t>(keyword));
    }
  }

  static constexpr bool isIntegerPackable(float f) {
    constexpr uint16_t kMaxInlineAbsValue = (1 << 11) - 1;

    auto i = static_cast<int32_t>(f);
    return static_cast<float>(i) == f && i >= -kMaxInlineAbsValue &&
        i <= +kMaxInlineAbsValue;
  }

  static constexpr uint16_t packInlineInteger(float value) {
    uint16_t isNegative = value < 0 ? 1 : 0;
    return static_cast<uint16_t>(
        (isNegative << 11) |
        (static_cast<int32_t>(value) * (isNegative != 0u ? -1 : 1)));
  }

  static constexpr float unpackInlineInteger(uint16_t value) {
    constexpr uint16_t kValueSignMask = 0b0000'1000'0000'0000;
    constexpr uint16_t kValueMagnitudeMask = 0b0000'0111'1111'1111;
    const bool isNegative = (value & kValueSignMask) != 0;
    return static_cast<float>(
        (value & kValueMagnitudeMask) * (isNegative ? -1 : 1));
  }

  SmallValueBuffer<4> buffer_;
  std::vector<std::vector<ExpressionNode>> expressionSlots_{};
  std::vector<uint16_t> freeSlots_{};
  // Buffer chunks orphaned when a buffer-indexed value is overwritten by an
  // expression. SmallValueBuffer has no free of its own, so we reclaim them
  // here for reuse by the next non-inlineable value (see storeValue).
  std::vector<uint16_t> freeBufferChunks_{};
};

} // namespace facebook::yoga
